/****************************************************************************
 *
 * MagLab — 磁图生成（Handle）
 *
 * 数据来源：DataRecorder 写出的 CSV（时间,经度,纬度,海拔,...,S1,S2,...），
 * 映射值为 S1 - S2，位置为第 2/3 列。
 *
 * 性能要点（2026-10 重构）：
 *   1. 生成过程完全在工作线程执行（QtConcurrent），GUI 只接收结果，
 *      通过 busy / progressText 让 UI 显示进度而不是卡死。
 *   2. Delaunay 的边去重由 O(E²) 改为按方向计数 O(E)。
 *   3. 栅格插值用均匀网格索引定位三角形，把
 *      O(像素 × 三角形数) 降为 O(像素 × 每格三角形数)。
 *   4. 像素直接写入 QImage 扫描线，不再逐点 setPixelColor。
 *
 ****************************************************************************/

#pragma once

#include <QImage>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QStringListModel>
#include <QVector>

#include <vector>
#include <atomic>
#include <memory>
#include <QtQml/qqmlregistration.h>

struct MagPoint {
    double lon;
    double lat;
    double mag;
};

struct Triangle {
    int a, b, c;   // MagPoint 的索引
};

// Forward decls
class Vehicle;

class Handle : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    Handle();
    virtual ~Handle();

    Q_PROPERTY(QString magmapPath     READ magmapPath     WRITE setmagmapPath NOTIFY magmapPathChanged)
    Q_PROPERTY(QString magmapFilePath READ magmapFilePath NOTIFY magmapFilePathChanged)
    Q_PROPERTY(bool    busy           READ busy           NOTIFY busyChanged)
    Q_PROPERTY(int     sampleCount    READ sampleCount    NOTIFY generationFinished)
    Q_PROPERTY(QString progressText   READ progressText   NOTIFY progressTextChanged)
    Q_PROPERTY(QString errorString    READ errorString    NOTIFY errorStringChanged)
    Q_PROPERTY(QVariantMap sourceInfo READ sourceInfo     NOTIFY magmapPathChanged)

    QString magmapPath()     const { return _magmapPath; }
    QString magmapFilePath() const { return _magmapFilePath; }
    bool    busy()           const { return _busy; }
    int     sampleCount()    const { return _sampleCount; }
    QString progressText()   const { return _progressText; }
    QString errorString()    const { return _errorString; }
    QVariantMap sourceInfo() const;

    void setmagmapPath(QString magmapPath);
    /// Selects a CSV without starting generation (used by the import preview).
    Q_INVOKABLE void selectSource(QString magmapPath);
    Q_INVOKABLE void generate() { _genMagmap(); }
    Q_INVOKABLE void regenerate() { _genMagmap(); }
    Q_INVOKABLE void cancelGeneration();

    void generateMagImage(QVector<MagPoint> samples, int N);
    std::vector<Triangle> delaunayTriangulation(QVector<MagPoint>& pts);
    bool inCircumcircle(const MagPoint& p,
                        const MagPoint& a,
                        const MagPoint& b,
                        const MagPoint& c);
    double interpolateGrid(double lon, double lat,
                           const QVector<MagPoint>& pts,
                           const std::vector<Triangle>& tris);
    QColor jetColor(double v, double vMin, double vMax);
    inline double orient2d(const MagPoint& a,
                           const MagPoint& b,
                           const MagPoint& c);
    bool barycentric(double x, double y,
                     const MagPoint& p1,
                     const MagPoint& p2,
                     const MagPoint& p3,
                     double& w1, double& w2, double& w3);

signals:
    void magmapPathChanged();
    void magmapFilePathChanged();
    void busyChanged();
    void progressTextChanged();
    void errorStringChanged();

    /// 生成成功结束时发出（signals 段内的函数名会原样成为 QML 的处理器名，
    /// QML 侧对应 onGenmagmapFinished）。失败时不发，只更新 errorString。
    void genmagmapFinished();

    /// 成功结束的别名（与 genmagmapFinished 同时发出，便于语义化绑定）
    void generationFinished();

private slots:
    void _genMagmap(void);
    void _onGenerationFinished();

private:
    QString _magmapPath;
    QString _magmapFilePath;
    QString _progressText;
    QString _errorString;
    bool    _busy        = false;
    int     _sampleCount = 0;

    // 上一次成功生成的数据：换分辨率重新出图时可直接复用，跳过重新三角剖分
    QVector<MagPoint>     _cachedPts;
    std::vector<Triangle> _cachedTris;

    // 范围信息（供兼容性路径 generateMagImage 写 txt 使用）
    double _lonMin = 0.0, _lonMax = 0.0, _latMin = 0.0, _latMax = 0.0;
    double _difmagMin = 0.0, _difmagMax = 0.0;

    // QFutureWatcher<MagmapResult> 的类型擦除（避免在头文件里包含 QtConcurrent）
    std::shared_ptr<std::atomic_bool> _cancel;
    void*   _watcher      = nullptr;
    quint64 _generationId = 0;
};
