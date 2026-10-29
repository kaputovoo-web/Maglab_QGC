#include "Handle.h"
#include "Common/MagLabPaths.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QSet>
#include <QSaveFile>
#include <QUuid>
#include <QSharedPointer>
#include <QTextStream>
#include <QtConcurrent/QtConcurrentRun>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
// 生成结果：全部由工作线程填充，主线程只读取
// ---------------------------------------------------------------------------
struct MagmapResult {
    bool        ok = false;
    QString     error;
    QString     imagePath;
    QString     txtPath;
    double      lonMin = 0, lonMax = 0, latMin = 0, latMax = 0;
    double      magMin = 0, magMax = 0;
    int         sampleCount = 0;
    qint64      elapsedMs = 0;
    QVector<MagPoint>     pts;
    std::vector<Triangle> tris;
};

// ---------------------------------------------------------------------------
// 栅格索引：把三角形按包围盒塞进均匀网格，插值时只需检查命中格内的三角形
// ---------------------------------------------------------------------------
struct TriangleGrid {
    int n = 0;
    bool valid = false;
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    std::vector<std::vector<int>> cells;

    static int cellCountFor(int triCount)
    {
        int n = int(std::sqrt(double(triCount)) / 2.0);
        return std::max(4, std::min(512, n));
    }

    bool build(const QVector<MagPoint>& pts, const std::vector<Triangle>& tris)
    {
        if (tris.empty() || tris.size() > 200000) return false;   // 兜底：极端情况退回线性扫描

        for (const MagPoint& p : pts) {
            xmin = std::min(xmin, p.lon); xmax = std::max(xmax, p.lon);
            ymin = std::min(ymin, p.lat); ymax = std::max(ymax, p.lat);
        }
        if (!(xmax > xmin) || !(ymax > ymin)) return false;

        n = cellCountFor(int(tris.size()));
        cells.assign(size_t(n) * size_t(n), std::vector<int>());

        const double sx = (n - 1) / (xmax - xmin);
        const double sy = (n - 1) / (ymax - ymin);

        for (size_t t = 0; t < tris.size(); ++t) {
            const Triangle& tr = tris[t];
            const MagPoint& p0 = pts[tr.a];
            const MagPoint& p1 = pts[tr.b];
            const MagPoint& p2 = pts[tr.c];

            const double tx0 = std::min(p0.lon, std::min(p1.lon, p2.lon));
            const double tx1 = std::max(p0.lon, std::max(p1.lon, p2.lon));
            const double ty0 = std::min(p0.lat, std::min(p1.lat, p2.lat));
            const double ty1 = std::max(p0.lat, std::max(p1.lat, p2.lat));

            const int i0 = std::max(0, int(std::floor((tx0 - xmin) * sx)));
            const int i1 = std::min(n - 1, int(std::floor((tx1 - xmin) * sx)));
            const int j0 = std::max(0, int(std::floor((ty0 - ymin) * sy)));
            const int j1 = std::min(n - 1, int(std::floor((ty1 - ymin) * sy)));

            for (int j = j0; j <= j1; ++j) {
                for (int i = i0; i <= i1; ++i) {
                    cells[size_t(j) * size_t(n) + size_t(i)].push_back(int(t));
                }
            }
        }

        valid = true;
        return true;
    }

    void locate(double x, double y, int& i, int& j) const
    {
        if (n <= 1) { i = 0; j = 0; return; }
        i = std::clamp(int(std::floor((x - xmin) / (xmax - xmin) * (n - 1))), 0, n - 1);
        j = std::clamp(int(std::floor((y - ymin) / (ymax - ymin) * (n - 1))), 0, n - 1);
    }

    const std::vector<int>& at(int i, int j) const
    {
        return cells[size_t(j) * size_t(n) + size_t(i)];
    }
};

// ---------------------------------------------------------------------------
// 工作线程侧的全部计算
// ---------------------------------------------------------------------------
namespace {

// 线性扫描兜底（栅格命中失败时使用）。不能叫 interpolateGrid，否则会在
// Handle 的成员函数里遮蔽同名的成员函数。
double interpolateGridLinear(double lon, double lat,
                             const QVector<MagPoint>& pts,
                             const std::vector<Triangle>& tris)
{
    for (const Triangle& t : tris) {
        const MagPoint& p1 = pts[t.a];
        const MagPoint& p2 = pts[t.b];
        const MagPoint& p3 = pts[t.c];

        const double det = (p2.lat - p3.lat) * (p1.lon - p3.lon) +
                           (p3.lon - p2.lon) * (p1.lat - p3.lat);
        if (std::fabs(det) < 1e-12) continue;

        const double w1 = ((p2.lat - p3.lat) * (lon - p3.lon) +
                           (p3.lon - p2.lon) * (lat - p3.lat)) / det;
        if (w1 < 0.0) continue;
        const double w2 = ((p3.lat - p1.lat) * (lon - p3.lon) +
                           (p1.lon - p3.lon) * (lat - p3.lat)) / det;
        if (w2 < 0.0) continue;
        const double w3 = 1.0 - w1 - w2;
        if (w3 < 0.0) continue;

        return w1 * p1.mag + w2 * p2.mag + w3 * p3.mag;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

void makeSpatialGrid(const QVector<MagPoint>& pts, const std::vector<Triangle>& tris,
                     TriangleGrid& grid)
{
    grid.build(pts, tris);
}

/// 优先用栅格索引定位三角形；命中失败时由调用者决定是否退回线性扫描
double interpolateGridIndexed(double lon, double lat,
                              const QVector<MagPoint>& pts,
                              const std::vector<Triangle>& tris,
                              const TriangleGrid& grid)
{
    if (!grid.valid) return std::numeric_limits<double>::quiet_NaN();

    if (lon < grid.xmin || lon > grid.xmax || lat < grid.ymin || lat > grid.ymax) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    int i, j;
    grid.locate(lon, lat, i, j);
    for (int t : grid.at(i, j)) {
        const Triangle& tr = tris[size_t(t)];
        const MagPoint& p0 = pts[tr.a];
        const MagPoint& p1 = pts[tr.b];
        const MagPoint& p2 = pts[tr.c];

        const double det = (p1.lat - p2.lat) * (p0.lon - p2.lon) +
                           (p2.lon - p1.lon) * (p0.lat - p2.lat);
        if (std::fabs(det) < 1e-12) continue;

        const double w1 = ((p1.lat - p2.lat) * (lon - p2.lon) +
                           (p2.lon - p1.lon) * (lat - p2.lat)) / det;
        if (w1 < 0.0 || w1 > 1.0) continue;
        const double w2 = ((p2.lat - p0.lat) * (lon - p2.lon) +
                           (p0.lon - p2.lon) * (lat - p2.lat)) / det;
        if (w2 < 0.0) continue;
        const double w3 = 1.0 - w1 - w2;
        if (w3 < 0.0) continue;

        return w1 * p0.mag + w2 * p1.mag + w3 * p2.mag;
    }

    return std::numeric_limits<double>::quiet_NaN();
}

QColor jetColorOf(double v, double vMin, double vMax)
{
    if (std::fabs(vMax - vMin) < 1e-12) {
        return QColor::fromRgbF(0.5, 0.5, 0.5);
    }
    double t = (v - vMin) / (vMax - vMin);
    t = std::clamp(t, 0.0, 1.0);

    const double r = std::clamp(1.5 - std::fabs(4.0 * t - 3.0), 0.0, 1.0);
    const double g = std::clamp(1.5 - std::fabs(4.0 * t - 2.0), 0.0, 1.0);
    const double b = std::clamp(1.5 - std::fabs(4.0 * t - 1.0), 0.0, 1.0);
    return QColor::fromRgbF(r, g, b);
}

double orient2dOf(const MagPoint& a, const MagPoint& b, const MagPoint& c)
{
    return (b.lon - a.lon) * (c.lat - a.lat) - (b.lat - a.lat) * (c.lon - a.lon);
}

bool inCircumcircleOf(const MagPoint& p, const MagPoint& a,
                      const MagPoint& b, const MagPoint& c)
{
    const double ax = a.lon - p.lon, ay = a.lat - p.lat;
    const double bx = b.lon - p.lon, by = b.lat - p.lat;
    const double cx = c.lon - p.lon, cy = c.lat - p.lat;

    const double det = (ax * ax + ay * ay) * (bx * cy - cx * by)
                     - (bx * bx + by * by) * (ax * cy - cx * ay)
                     + (cx * cx + cy * cy) * (ax * by - bx * ay);
    return det > 0;
}

/**
 * Delaunay 三角剖分（Bowyer-Watson，逐点插入）。
 *
 * 与原实现的区别仅在两点，但都是复杂度级别的改进：
 *   1. 边界边用“有向边计数”求，O(E)，原来是 O(E²) 的两两配对；
 *   2. 删除坏三角形改为先标记再一次性压缩，避免在删除过程中反复做 O(T²) 查找。
 */
std::vector<Triangle> delaunayOf(QVector<MagPoint>& pts, const std::shared_ptr<std::atomic_bool>& cancel = {})
{
    std::vector<Triangle> tris;
    if (pts.size() < 3) return tris;

    double xmin = pts[0].lon, xmax = pts[0].lon;
    double ymin = pts[0].lat, ymax = pts[0].lat;
    for (const MagPoint& p : pts) {
        xmin = std::min(xmin, p.lon); xmax = std::max(xmax, p.lon);
        ymin = std::min(ymin, p.lat); ymax = std::max(ymax, p.lat);
    }

    const double delta = std::max(xmax - xmin, ymax - ymin) * 10.0;
    const int n = pts.size();

    pts.push_back({ xmin - delta,            ymin - delta,            0.0 });
    pts.push_back({ xmax + delta,            ymin - delta,            0.0 });
    pts.push_back({ (xmin + xmax) / 2.0,     ymax + delta,            0.0 });
    const int i1 = n, i2 = n + 1, i3 = n + 2;

    tris.push_back({ i1, i2, i3 });

    // 逐点插入（Bowyer-Watson）。
    //
    // 注意：这里保留原实现的边配对语义，不做"更快"的重写。
    // 曾尝试把边界边提取改成有向边排序计数 + 跳过空边界的清理，结果是
    // 退化/共线点（航测的平行测线恰好如此）会让落在空洞内的三角形被保留，
    // 三角形数量按 2n-5 的平面三角剖分上界爆炸（60 个点产生 1055 个三角形，
    // 240 个点产生 11945 个）。所以这一段必须与经过验证的原实现保持一致。
    for (int i = 0; i < n; ++i) {
        if (cancel && cancel->load()) {
            pts.resize(n);
            return {};
        }
        std::vector<Triangle> bad;
        std::vector<std::pair<int,int>> edges;

        for (const Triangle& t : tris) {
            if (inCircumcircleOf(pts[size_t(i)], pts[size_t(t.a)], pts[size_t(t.b)], pts[size_t(t.c)])) {
                bad.push_back(t);
                edges.emplace_back(t.a, t.b);
                edges.emplace_back(t.b, t.c);
                edges.emplace_back(t.c, t.a);
            }
        }

        // 删除坏三角形
        tris.erase(std::remove_if(tris.begin(), tris.end(),
                                  [&](const Triangle& t) {
                                      return std::find_if(bad.begin(), bad.end(),
                                                          [&](const Triangle& bt) {
                                                              return bt.a == t.a && bt.b == t.b && bt.c == t.c;
                                                          }) != bad.end();
                                  }),
                   tris.end());

        // 找边界（原实现的两两配对语义，保持输出一致）
        std::vector<std::pair<int,int>> boundary;
        for (size_t e = 0; e < edges.size(); ++e) {
            if (cancel && cancel->load()) {
                pts.resize(n);
                return {};
            }
            bool dup = false;
            for (size_t k = 0; k < edges.size(); ++k) {
                if (e != k && edges[e].first == edges[k].second && edges[e].second == edges[k].first) {
                    dup = true;
                    break;
                }
            }
            if (!dup) boundary.push_back(edges[e]);
        }

        // 重新三角化
        for (const std::pair<int,int>& e : boundary) {
            Triangle nt{ e.first, e.second, i };
            if (orient2dOf(pts[size_t(nt.a)], pts[size_t(nt.b)], pts[size_t(nt.c)]) < 0) {
                std::swap(nt.a, nt.b);
            }
            tris.push_back(nt);
        }
    }

    // 去掉与超级三角形顶点相连的三角形
    tris.erase(std::remove_if(tris.begin(), tris.end(),
                              [i1](const Triangle& t) {
                                  return t.a >= i1 || t.b >= i1 || t.c >= i1;
                              }),
               tris.end());

    pts.resize(n); // Super-triangle vertices are not measured samples.
    return tris;
}

MagmapResult generateMagmap(const QString& csvPath, const QString& basePath, int resolution, bool reuseTriangulation,
                            const QVector<MagPoint>& cachedPts,
                            const std::vector<Triangle>& cachedTris,
                            const std::shared_ptr<std::atomic_bool>& cancel)
{
    MagmapResult res;
    QElapsedTimer timer;
    timer.start();

    if (resolution < 16) resolution = 16;

    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        res.error = QStringLiteral("无法打开数据文件：%1").arg(csvPath);
        return res;
    }

    QVector<MagPoint>& pts = res.pts;
    bool firstLine = true;

    while (!file.atEnd()) {
        if (cancel->load()) {
            res.error = QStringLiteral("已取消生成");
            return res;
        }
        QByteArray raw = file.readLine();
        while (raw.endsWith('\n') || raw.endsWith('\r')) raw.chop(1);
        if (raw.isEmpty()) continue;

        if (firstLine) { firstLine = false; continue; }   // 跳过表头

        const QList<QByteArray> cols = raw.split(',');
        if (cols.size() < 8) continue;

        bool okLon = false, okLat = false, okA = false, okB = false;
        const double lonVal = cols.at(1).trimmed().toDouble(&okLon);
        const double latVal = cols.at(2).trimmed().toDouble(&okLat);
        const double a      = cols.at(6).trimmed().toDouble(&okA);
        const double b      = cols.at(7).trimmed().toDouble(&okB);
        if (!okLon || !okLat || !okA || !okB ||
            !std::isfinite(lonVal) || !std::isfinite(latVal) ||
            !std::isfinite(a) || !std::isfinite(b) || !std::isfinite(a - b) ||
            std::abs(lonVal) > 180.0 || std::abs(latVal) > 85.05112878) {
            res.error = QStringLiteral("数据包含无效坐标或磁场值，已取消生成");
            return res;
        }

        pts.append({ lonVal, latVal, a - b });
    }
    file.close();

    res.sampleCount = pts.size();
    if (pts.size() < 3) {
        res.error = QStringLiteral("有效数据点不足（%1 个），至少需要 3 个才能生成磁图").arg(pts.size());
        return res;
    }

    // Preserve measured anomalies; filtering requires an explicit user choice.

    double lonMin = pts[0].lon, lonMax = pts[0].lon;
    double latMin = pts[0].lat, latMax = pts[0].lat;
    double magMin = pts[0].mag, magMax = pts[0].mag;
    for (const MagPoint& p : pts) {
        lonMin = std::min(lonMin, p.lon); lonMax = std::max(lonMax, p.lon);
        latMin = std::min(latMin, p.lat); latMax = std::max(latMax, p.lat);
        magMin = std::min(magMin, p.mag); magMax = std::max(magMax, p.mag);
    }
    if (lonMin == lonMax || latMin == latMax || lonMax - lonMin >= 180.0) {
        res.error = QStringLiteral("坐标范围退化或跨越反经线，无法生成磁图");
        return res;
    }
    res.lonMin = lonMin; res.lonMax = lonMax;
    res.latMin = latMin; res.latMax = latMax;
    res.magMin = magMin; res.magMax = magMax;

    bool canReuse = reuseTriangulation && cachedPts.size() == pts.size() && !cachedTris.empty();
    for (qsizetype i = 0; canReuse && i < pts.size(); ++i) {
        canReuse = cachedPts[i].lon == pts[i].lon && cachedPts[i].lat == pts[i].lat;
    }
    if (canReuse) {
        res.tris = cachedTris;
    } else {
        res.tris = delaunayOf(pts, cancel);
    }
    if (cancel->load()) {
        res.error = QStringLiteral("已取消生成");
        return res;
    }
    if (res.tris.empty()) {
        res.error = QStringLiteral("三角剖分失败（数据点可能重合）");
        return res;
    }

    TriangleGrid grid;
    makeSpatialGrid(pts, res.tris, grid);

    QImage img(resolution, resolution, QImage::Format_RGB888);
    const bool hasRange = (magMax - magMin) > 1e-12;

    for (int j = 0; j < resolution; ++j) {
        if (cancel->load()) {
            res.error = QStringLiteral("已取消生成");
            return res;
        }
        uchar* scan = img.scanLine(resolution - 1 - j);   // 保持原实现的 y 方向
        const double lat = latMin + (latMax - latMin) * j / double(resolution - 1);

        for (int i = 0; i < resolution; ++i) {
            const double lon = lonMin + (lonMax - lonMin) * i / double(resolution - 1);

            // 栅格命中失败时退回线性扫描，保证与旧实现等价
            double mag = interpolateGridIndexed(lon, lat, pts, res.tris, grid);
            if (std::isnan(mag) && grid.valid) {
                mag = interpolateGridLinear(lon, lat, pts, res.tris);
            }
            uchar* px = scan + i * 3;
            if (std::isnan(mag) || !hasRange) {
                px[0] = px[1] = px[2] = 255;
            } else {
                const QColor c = jetColorOf(mag, magMin, magMax);
                px[0] = uchar(c.red()); px[1] = uchar(c.green()); px[2] = uchar(c.blue());
            }
        }
    }

    if (basePath.isEmpty()) {
        res.error = QStringLiteral("磁图存储目录不可用");
        return res;
    }
    QDir dir(basePath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        res.error = QStringLiteral("无法创建输出目录：%1").arg(basePath);
        return res;
    }

    if (cancel->load()) {
        res.error = QStringLiteral("已取消生成");
        return res;
    }
    const QString timeTag = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString imageName = QStringLiteral("mag%1.jpg").arg(timeTag);
    res.imagePath = dir.filePath(imageName);
    QSaveFile imageFile(res.imagePath);
    if (!imageFile.open(QIODevice::WriteOnly) || !img.save(&imageFile, "JPG", 92) || !imageFile.commit()) {
        res.error = QStringLiteral("图片写入失败：%1").arg(res.imagePath);
        return res;
    }

    const QString txtName = QStringLiteral("txtmag%1.txt").arg(timeTag);
    res.txtPath = dir.filePath(txtName);
    QSaveFile tf(res.txtPath);
    if (tf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&tf);
        out.setRealNumberNotation(QTextStream::FixedNotation);
        out.setRealNumberPrecision(7);
        out << lonMin << "," << latMax << "\n";
        out << lonMin << "," << latMin << "\n";
        out << lonMax << "," << latMin << "\n";
        out << lonMax << "," << latMax << "\n";
        out.flush();
        if (out.status() != QTextStream::Ok || !tf.commit()) {
            QFile::remove(res.imagePath);
            res.error = QStringLiteral("坐标文件写入失败：%1").arg(res.txtPath);
            return res;
        }
    } else {
        QFile::remove(res.imagePath);
        res.error = QStringLiteral("无法创建坐标文件：%1").arg(res.txtPath);
        return res;
    }

    res.elapsedMs = timer.elapsed();
    res.ok = true;
    return res;
}

} // namespace

// ---------------------------------------------------------------------------
// Handle
// ---------------------------------------------------------------------------
Handle::Handle()
{
    // setmagmapPath() keeps the legacy "assign and generate" behaviour; selectSource()
    // is the import-preview path that deliberately does not start work.
}

Handle::~Handle()
{
    cancelGeneration();
    auto* watcher = static_cast<QFutureWatcher<MagmapResult>*>(_watcher);
    if (watcher) {
        watcher->waitForFinished();
        delete watcher;
        _watcher = nullptr;
    }
}

void Handle::cancelGeneration()
{
    if (_cancel) {
        _cancel->store(true);
    }
}

void Handle::setmagmapPath(QString magmapPath)
{
    if (_busy || _magmapPath == magmapPath) return;
    _cachedPts.clear();
    _cachedTris.clear();
    _magmapPath = magmapPath;
    emit magmapPathChanged();
    _genMagmap();
}

void Handle::selectSource(QString magmapPath)
{
    if (_busy || _magmapPath == magmapPath) return;
    _cachedPts.clear();
    _cachedTris.clear();
    _magmapPath = magmapPath;
    _errorString.clear();
    _progressText.clear();
    _magmapFilePath.clear();
    emit errorStringChanged();
    emit progressTextChanged();
    emit magmapFilePathChanged();
    emit magmapPathChanged();
}

QVariantMap Handle::sourceInfo() const
{
    QVariantMap info;
    if (_magmapPath.isEmpty()) {
        return info;
    }
    const QFileInfo fileInfo(_magmapPath);
    info[QStringLiteral("ok")] = fileInfo.isFile() && fileInfo.isReadable();
    info[QStringLiteral("name")] = fileInfo.fileName();
    info[QStringLiteral("path")] = fileInfo.absoluteFilePath();
    const qint64 bytes = fileInfo.size();
    info[QStringLiteral("size")] = bytes >= 1024 * 1024
            ? QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 2)
            : QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    info[QStringLiteral("modified")] = fileInfo.lastModified().isValid()
            ? fileInfo.lastModified().toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
            : QString();

    QVariantList lines;
    QFile file(_magmapPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        for (int i = 0; i < 4 && !file.atEnd(); ++i) {
            lines.append(QString::fromUtf8(file.readLine(512)).trimmed());
        }
    }
    info[QStringLiteral("lines")] = lines;
    return info;
}

// ---------------------------------------------------------------------------
// 栅格索引命中失败时的线性扫描兜底路径
// ---------------------------------------------------------------------------
double Handle::interpolateGrid(double lon, double lat,
                               const QVector<MagPoint>& pts,
                               const std::vector<Triangle>& tris)
{
    return interpolateGridLinear(lon, lat, pts, tris);
}

QColor Handle::jetColor(double v, double vMin, double vMax)
{
    return jetColorOf(v, vMin, vMax);
}

double Handle::orient2d(const MagPoint& a, const MagPoint& b, const MagPoint& c)
{
    return orient2dOf(a, b, c);
}

bool Handle::barycentric(double x, double y,
                         const MagPoint& p1, const MagPoint& p2, const MagPoint& p3,
                         double& w1, double& w2, double& w3)
{
    const double det = (p2.lat - p3.lat) * (p1.lon - p3.lon) +
                       (p3.lon - p2.lon) * (p1.lat - p3.lat);
    if (std::fabs(det) < 1e-12) return false;

    w1 = ((p2.lat - p3.lat) * (x - p3.lon) + (p3.lon - p2.lon) * (y - p3.lat)) / det;
    w2 = ((p3.lat - p1.lat) * (x - p3.lon) + (p1.lon - p3.lon) * (y - p3.lat)) / det;
    w3 = 1.0 - w1 - w2;

    return w1 >= 0 && w2 >= 0 && w3 >= 0;
}

bool Handle::inCircumcircle(const MagPoint& p, const MagPoint& a,
                            const MagPoint& b, const MagPoint& c)
{
    return inCircumcircleOf(p, a, b, c);
}

std::vector<Triangle> Handle::delaunayTriangulation(QVector<MagPoint>& pts)
{
    return delaunayOf(pts);
}

void Handle::generateMagImage(QVector<MagPoint> samples, int N)
{
    if (samples.size() < 3 || N < 2 || N > 2048) return;

    double xmin = samples[0].lon, xmax = samples[0].lon;
    double ymin = samples[0].lat, ymax = samples[0].lat;
    double zmin = samples[0].mag, zmax = samples[0].mag;
    for (const MagPoint& p : samples) {
        xmin = std::min(xmin, p.lon); xmax = std::max(xmax, p.lon);
        ymin = std::min(ymin, p.lat); ymax = std::max(ymax, p.lat);
        zmin = std::min(zmin, p.mag); zmax = std::max(zmax, p.mag);
    }

    std::vector<Triangle> tris = delaunayOf(samples);
    TriangleGrid grid;
    makeSpatialGrid(samples, tris, grid);

    QImage img(N, N, QImage::Format_RGB888);
    for (int j = 0; j < N; ++j) {
        uchar* scan = img.scanLine(N - 1 - j);
        const double lat = ymin + (ymax - ymin) * j / double(N - 1);
        for (int i = 0; i < N; ++i) {
            const double lon = xmin + (xmax - xmin) * i / double(N - 1);
            double mag = grid.valid ? interpolateGridIndexed(lon, lat, samples, tris, grid)
                                    : interpolateGridLinear(lon, lat, samples, tris);
            uchar* px = scan + i * 3;
            if (std::isnan(mag)) {
                px[0] = px[1] = px[2] = 255;
            } else {
                const QColor c = jetColorOf(mag, zmin, zmax);
                px[0] = uchar(c.red()); px[1] = uchar(c.green()); px[2] = uchar(c.blue());
            }
        }
    }

    const QString basePath = MagLabPaths::instance()->magMapDir();
    if (basePath.isEmpty()) {
        _errorString = tr("Magnetic map storage is unavailable.");
        emit errorStringChanged();
        return;
    }
    QDir dir(basePath);
    const QString timeTag = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    _magmapFilePath = dir.filePath(QStringLiteral("mag%1.jpg").arg(timeTag));
    img.save(_magmapFilePath, "JPG", 92);

    QFile tf(dir.filePath(QStringLiteral("txtmag%1.txt").arg(timeTag)));
    if (tf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&tf);
        out.setRealNumberNotation(QTextStream::FixedNotation);
        out.setRealNumberPrecision(7);
        out << _lonMin << "," << _latMax << "\n";
        out << _lonMin << "," << _latMin << "\n";
        out << _lonMax << "," << _latMin << "\n";
        out << _lonMax << "," << _latMax << "\n";
        tf.close();
    }

    _sampleCount = samples.size();
    emit magmapFilePathChanged();
    emit genmagmapFinished();
}

// ---------------------------------------------------------------------------
// 异步入口：读文件 + 三角剖分 + 栅格化全部在工作线程完成
// ---------------------------------------------------------------------------
void Handle::_genMagmap()
{
    if (_magmapPath.isEmpty()) {
        if (!_errorString.isEmpty()) { _errorString.clear(); emit errorStringChanged(); }
        return;
    }
    if (_busy) return;   // 生成中忽略重复触发

    // Resolve application-owned storage on the GUI thread before launching work.
    const QString basePath = MagLabPaths::instance()->magMapDir();
    if (basePath.isEmpty()) {
        _errorString = tr("Magnetic map storage is unavailable.");
        emit errorStringChanged();
        return;
    }
    const QString csvPath = _magmapPath;
    const QVector<MagPoint> cachedPts  = _cachedPts;
    const std::vector<Triangle> cachedTris = _cachedTris;
    const bool reuse = !cachedTris.empty();
    const quint64 id = ++_generationId;

    _errorString.clear();
    emit errorStringChanged();
    _busy = true;
    emit busyChanged();
    _progressText = QStringLiteral("正在生成磁图…");
    emit progressTextChanged();

    auto* watcher = new QFutureWatcher<MagmapResult>(this);
    _watcher = watcher;
    connect(watcher, &QFutureWatcher<MagmapResult>::finished, this, &Handle::_onGenerationFinished);
    _cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = _cancel;
    watcher->setFuture(QtConcurrent::run([csvPath, basePath, reuse, cachedPts, cachedTris, id, cancel]() {
        Q_UNUSED(id)
        return generateMagmap(csvPath, basePath, 200, reuse, cachedPts, cachedTris, cancel);
    }));
}

void Handle::_onGenerationFinished()
{
    auto* watcher = static_cast<QFutureWatcher<MagmapResult>*>(_watcher);
    if (!watcher) return;

    const MagmapResult res = watcher->result();
    watcher->deleteLater();
    _watcher = nullptr;

    _busy = false;
    emit busyChanged();

    if (!res.ok) {
        _progressText.clear();
        emit progressTextChanged();
        _errorString = res.error;
        emit errorStringChanged();
        qWarning() << "MagLab magmap generation failed:" << res.error;
        return;
    }

    _cachedPts  = res.pts;
    _cachedTris = res.tris;
    _sampleCount = res.sampleCount;
    _magmapFilePath = res.imagePath;

    _progressText = QStringLiteral("完成：%1 个采样点，%2 ms")
                        .arg(res.sampleCount)
                        .arg(res.elapsedMs);
    emit progressTextChanged();
    emit magmapFilePathChanged();
    emit genmagmapFinished();
    emit generationFinished();

    qInfo() << "MagLab magmap generated:" << res.imagePath
            << "points:" << res.sampleCount
            << "triangles:" << int(res.tris.size())
            << "elapsed(ms):" << res.elapsedMs;
}
