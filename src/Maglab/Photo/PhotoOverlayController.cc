#include "PhotoOverlayController.h"

#include "Common/MagLabPaths.h"
#include "Archive/MissionArchiveManager.h"
#include "GeoCameraProjector.h"

#include <QFont>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>
#include <QLinearGradient>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPolygonF>
#include <QSaveFile>
#include <QTextStream>
#include <QTransform>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>

namespace {

struct Corner {
    double lon = 0.0;
    double lat = 0.0;
};

struct PhotoPose {
    double lon = 0.0;
    double lat = 0.0;
    double altitude = 0.0;
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    double agl = 0.0;
    bool validGeo = false;
    bool validAttitude = false;
};

QString sidecarFor(const QString& photoPath)
{
    const QFileInfo info(photoPath);
    return QDir(MagLabPaths::instance()->photoMetaDir()).filePath(
                QStringLiteral("txt%1.txt").arg(info.completeBaseName()));
}

QString legacyAdjacentSidecar(const QString& photoPath)
{
    const QFileInfo info(photoPath);
    return info.dir().filePath(QStringLiteral("txt%1.txt").arg(info.completeBaseName()));
}

bool readCorners(const QString& path, Corner corners[4], double& minValue, double& maxValue, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QObject::tr("无法读取磁图角点文件：%1").arg(path);
        return false;
    }
    QTextStream stream(&file);
    int index = 0;
    while (!stream.atEnd() && index < 4) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;
        const QStringList fields = line.split(QLatin1Char(','));
        if (fields.size() < 2) continue;
        bool lonOk = false;
        bool latOk = false;
        const double lon = fields.at(0).trimmed().toDouble(&lonOk);
        const double lat = fields.at(1).trimmed().toDouble(&latOk);
        if (!lonOk || !latOk || !std::isfinite(lon) || !std::isfinite(lat) ||
            lon < -180.0 || lon > 180.0 || lat < -90.0 || lat > 90.0) {
            error = QObject::tr("磁图角点文件包含无效经纬度：%1").arg(path);
            return false;
        }
        corners[index++] = { lon, lat };
    }
    if (!stream.atEnd()) {
        const QStringList range = stream.readLine().trimmed().split(QLatin1Char(','));
        if (range.size() >= 2) {
            bool minOk = false;
            bool maxOk = false;
            const double minParsed = range.at(0).trimmed().toDouble(&minOk);
            const double maxParsed = range.at(1).trimmed().toDouble(&maxOk);
            if (minOk && maxOk && std::isfinite(minParsed) && std::isfinite(maxParsed)) {
                minValue = minParsed;
                maxValue = maxParsed;
            }
        }
    }
    if (index != 4) {
        error = QObject::tr("磁图角点文件必须包含 4 行：%1").arg(path);
        return false;
    }
    return true;
}

bool readPose(const QString& path, PhotoPose& pose, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QObject::tr("无法读取照片位姿文件：%1").arg(path);
        return false;
    }
    QTextStream stream(&file);
    const QStringList geo = stream.readLine().trimmed().split(QLatin1Char(','), Qt::KeepEmptyParts);
    if (geo.size() >= 2) {
        bool lonOk = false;
        bool latOk = false;
        pose.lon = geo.at(0).trimmed().toDouble(&lonOk);
        pose.lat = geo.at(1).trimmed().toDouble(&latOk);
        pose.validGeo = lonOk && latOk && std::isfinite(pose.lon) && std::isfinite(pose.lat) &&
                        pose.lon >= -180.0 && pose.lon <= 180.0 && pose.lat >= -90.0 && pose.lat <= 90.0;
    }
    const QStringList attitude = stream.readLine().trimmed().split(QLatin1Char(','), Qt::KeepEmptyParts);
    if (attitude.size() >= 3) {
        bool rollOk = false;
        bool pitchOk = false;
        bool yawOk = false;
        pose.roll = attitude.at(0).trimmed().toDouble(&rollOk);
        pose.pitch = attitude.at(1).trimmed().toDouble(&pitchOk);
        pose.yaw = attitude.at(2).trimmed().toDouble(&yawOk);
        pose.validAttitude = rollOk && pitchOk && yawOk;
    }
    if (!stream.atEnd()) {
        const QStringList altitudeFields = stream.readLine().trimmed().split(QLatin1Char(','), Qt::KeepEmptyParts);
        bool altitudeOk = false;
        pose.altitude = altitudeFields.value(0).trimmed().toDouble(&altitudeOk);
        if (!altitudeOk) pose.altitude = 0.0;
        // Optional extension: altitude,AGL on line 3, or AGL on line 4.
        if (altitudeFields.size() >= 2) {
            bool aglOk = false;
            const double parsedAgl = altitudeFields.at(1).trimmed().toDouble(&aglOk);
            if (aglOk && std::isfinite(parsedAgl) && parsedAgl > 0.0) pose.agl = parsedAgl;
        }
    }
    if (pose.agl <= 0.0 && !stream.atEnd()) {
        bool aglOk = false;
        const double parsedAgl = stream.readLine().trimmed().toDouble(&aglOk);
        if (aglOk && std::isfinite(parsedAgl) && parsedAgl > 0.0) pose.agl = parsedAgl;
    }
    if (!pose.validGeo) {
        error = QObject::tr("照片位姿文件缺少有效经纬度：%1").arg(path);
        return false;
    }
    if (!pose.validAttitude) {
        error = QObject::tr("照片位姿文件缺少姿态角：%1").arg(path);
        return false;
    }
    return true;
}

PhotoOverlayResult buildOverlay(const QString& photoPath, const QString& magImagePath,
                                double hfov, double vfov, double heightAGL,
                                double yawOffset, double transparency,
                                double requestedMin, double requestedMax,
                                const QString& outputPath,
                                const std::shared_ptr<std::atomic_bool>& cancel)
{
    PhotoOverlayResult result;
    result.outputPath = outputPath;
    if (cancel && cancel->load()) {
        result.cancelled = true;
        return result;
    }

    QImage photo(photoPath);
    QImage mag(magImagePath);
    if (photo.isNull()) {
        result.error = QObject::tr("无法读取照片：%1").arg(photoPath);
        return result;
    }
    if (mag.isNull()) {
        result.error = QObject::tr("无法读取磁图：%1").arg(magImagePath);
        return result;
    }

    const QFileInfo magInfo(magImagePath);
    const QString magSidecar = magInfo.dir().filePath(
                QStringLiteral("txt%1.txt").arg(magInfo.completeBaseName()));
    Corner corners[4];
    double mapMinValue = requestedMin;
    double mapMaxValue = requestedMax;
    QString error;
    if (!readCorners(magSidecar, corners, mapMinValue, mapMaxValue, error)) {
        result.error = error;
        return result;
    }

    PhotoPose pose;
    const QString photoSidecar = QFileInfo::exists(sidecarFor(photoPath))
            ? sidecarFor(photoPath) : legacyAdjacentSidecar(photoPath);
    if (!readPose(photoSidecar, pose, error)) {
        result.error = error;
        return result;
    }

    GeoCameraProjector::CameraIntrinsics K =
        GeoCameraProjector::intrinsicsFromFov(photo.width(), photo.height(), hfov, vfov);
    GeoCameraProjector::CameraPose cameraPose;
    cameraPose.refLatDeg = pose.lat;
    cameraPose.refLonDeg = pose.lon;
    cameraPose.refAltM = 0.0;
    cameraPose.heightAGL = pose.agl > 0.0 ? pose.agl : heightAGL;
    cameraPose.rollDeg = pose.roll;
    cameraPose.pitchDeg = pose.pitch;
    cameraPose.yawDeg = pose.yaw + yawOffset;

    QPointF projected[4];
    for (int i = 0; i < 4; ++i) {
        if (cancel && cancel->load()) {
            result.cancelled = true;
            return result;
        }
        if (!GeoCameraProjector::geoToPixelByPose(corners[i].lat, corners[i].lon,
                                                  cameraPose.refAltM, cameraPose, K,
                                                  projected[i])) {
            result.error = QObject::tr("磁图角点无法投影到照片，检查航高、FOV 和姿态数据");
            return result;
        }
    }

    QPolygonF source;
    source << QPointF(0, 0)
           << QPointF(0, mag.height())
           << QPointF(mag.width(), mag.height())
           << QPointF(mag.width(), 0);
    QPolygonF target;
    target << projected[0] << projected[1] << projected[2] << projected[3];

    // If the projected map extends outside the photo, scale it around its
    // bounding-box centre so the complete map is visible. This is a display
    // fit for the overlay image, not a replacement for camera calibration.
    QRectF mapBounds = target.boundingRect();
    const qreal margin = 8.0;
    if (mapBounds.left() < margin || mapBounds.top() < margin ||
        mapBounds.right() > photo.width() - margin ||
        mapBounds.bottom() > photo.height() - margin) {
        const qreal sx = (photo.width() - 2 * margin) / qMax(1.0, mapBounds.width());
        const qreal sy = (photo.height() - 2 * margin) / qMax(1.0, mapBounds.height());
        const qreal scale = qMin(sx, sy);
        const QPointF bc = mapBounds.center();
        const QPointF pc(photo.width() / 2.0, photo.height() / 2.0);
        for (QPointF& point : target) {
            point = pc + (point - bc) * scale;
        }
    }

    QTransform transform;
    if (!QTransform::quadToQuad(source, target, transform)) {
        result.error = QObject::tr("磁图透视变换失败");
        return result;
    }

    const int left = 160;
    const int top = 70;
    const int bottomAxis = 200;
    const int rightBar = 260;
    QImage output(photo.width() + left + rightBar,
                  photo.height() + top + bottomAxis,
                  QImage::Format_RGB32);
    output.fill(Qt::white);
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawImage(left, top, photo);

    painter.save();
    // Never paint the warped magnetic map over the axes or the colour bar.
    painter.setClipRect(left, top, photo.width(), photo.height());
    painter.translate(left, top);
    painter.setOpacity(qBound(0.0, transparency, 1.0));
    painter.setTransform(transform, true);
    painter.drawImage(0, 0, mag);
    painter.restore();

    double lonMin = corners[0].lon, lonMax = corners[0].lon;
    double latMin = corners[0].lat, latMax = corners[0].lat;
    for (int i = 1; i < 4; ++i) {
        lonMin = qMin(lonMin, corners[i].lon);
        lonMax = qMax(lonMax, corners[i].lon);
        latMin = qMin(latMin, corners[i].lat);
        latMax = qMax(latMax, corners[i].lat);
    }

    QFont axisFont(QStringLiteral("Times New Roman"), 18, QFont::Bold);
    QFont titleFont(QStringLiteral("Times New Roman"), 22, QFont::Bold);
    painter.setFont(axisFont);
    painter.setPen(QColor(25, 38, 55));
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int ticks = 5;
    for (int i = 0; i <= ticks; ++i) {
        const int x = left + i * (photo.width() - 1) / ticks;
        painter.setPen(QPen(QColor(25, 38, 55), 1.4));
        painter.drawLine(x, top + photo.height(), x, top + photo.height() + 7);
        const double lon = lonMin + (lonMax - lonMin) * i / ticks;
        painter.save();
        painter.translate(x - 2, top + photo.height() + 10);
        painter.rotate(45);
        painter.drawText(0, 0, QString::number(lon, 'f', 6));
        painter.restore();
    }
    for (int i = 0; i <= ticks; ++i) {
        const int y = top + i * (photo.height() - 1) / ticks;
        painter.setPen(QPen(QColor(25, 38, 55), 1.4));
        painter.drawLine(left - 7, y, left, y);
        const double lat = latMax - (latMax - latMin) * i / ticks;
        painter.drawText(QRectF(2, y - 16, left - 18, 32),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(lat, 'f', 6));
    }
    painter.setFont(titleFont);
    painter.drawText(QRectF(left, output.height() - 32, photo.width(), 28),
                     Qt::AlignHCenter | Qt::AlignVCenter, QStringLiteral("Longitude"));
    painter.save();
    painter.translate(22, top + photo.height() / 2);
    painter.rotate(-90);
    painter.drawText(QRectF(-140, -14, 280, 28), Qt::AlignCenter, QStringLiteral("Latitude"));
    painter.restore();

    if (std::isfinite(mapMinValue) && std::isfinite(mapMaxValue) &&
        mapMaxValue > mapMinValue) {
        const int barX = left + photo.width() + 42;
        const int barY = top;
        const int barW = 20;
        const int barH = photo.height();
        const int steps = 128;
        for (int i = 0; i < steps; ++i) {
            const double t = 1.0 - double(i) / double(steps - 1);
            const double x = qBound(0.0, t, 1.0);
            const auto channel = [x](double center) {
                return qBound(0, qRound(255.0 * qBound(0.0, 1.5 - qAbs(4.0 * x - center), 1.0)), 255);
            };
            painter.fillRect(barX, barY + i * barH / steps, barW, barH / steps + 1,
                             QColor(channel(3.0), channel(2.0), channel(1.0)));
        }
        painter.setPen(QPen(QColor(25, 38, 55), 1.4));
        painter.drawRect(barX, barY, barW, barH);
        painter.setFont(titleFont);
        painter.drawText(QRectF(barX, barY - 42, 100, 28), Qt::AlignLeft, QStringLiteral("B (nT)"));
        painter.setFont(axisFont);
        painter.drawText(QRectF(barX + barW + 10, barY - 12, 70, 24),
                         Qt::AlignLeft | Qt::AlignVCenter, QString::number(mapMaxValue, 'f', 1));
        painter.drawText(QRectF(barX + barW + 10, barY + barH / 2 - 12, 70, 24),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::number((mapMinValue + mapMaxValue) * 0.5, 'f', 1));
        painter.drawText(QRectF(barX + barW + 10, barY + barH - 12, 70, 24),
                         Qt::AlignLeft | Qt::AlignVCenter, QString::number(mapMinValue, 'f', 1));
    }
    // Ground scale bar. The estimate uses the nadir horizontal footprint
    // derived from the active AGL and horizontal FOV.
    const double effectiveAgl = pose.agl > 0.0 ? pose.agl : heightAGL;
    if (effectiveAgl > 0.0 && photo.width() > 0) {
        const double metersPerPixel = 2.0 * effectiveAgl
                * std::tan(hfov * M_PI / 360.0) / double(photo.width());
        double desiredMeters = photo.width() * 0.18 * metersPerPixel;
        const double exponent = std::pow(10.0, std::floor(std::log10(qMax(desiredMeters, 0.001))));
        const double base = desiredMeters / exponent;
        const double nice = base < 1.5 ? 1.0 : (base < 3.5 ? 2.0 : (base < 7.5 ? 5.0 : 10.0));
        const double meters = qMax(0.001, nice * exponent);
        double scalePx = meters / metersPerPixel;
        scalePx = qMin(scalePx, photo.width() * 0.35);
        if (scalePx >= 12.0) {
            const int x2 = left + photo.width() - 34;
            const int x1 = x2 - int(scalePx);
            const int y = top + photo.height() - 30;
            QFont scaleFont(QStringLiteral("Times New Roman"), 16, QFont::Bold);
            QFontMetrics fm(scaleFont);
            const QString label = QStringLiteral("%1 m").arg(meters, 0, 'g', 3);
            painter.setPen(QPen(QColor(0, 124, 138), 3.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(x1, y, x2, y);
            painter.drawLine(x1, y - 9, x1, y);
            painter.drawLine(x2, y - 9, x2, y);
            painter.setFont(scaleFont);
            const int textW = fm.horizontalAdvance(label) + 10;
            painter.drawText(QRectF((x1 + x2) / 2 - textW / 2, y - 34, textW, 28),
                             Qt::AlignHCenter | Qt::AlignVCenter, label);
        }
    }

    painter.end();

    if (cancel && cancel->load()) {
        result.cancelled = true;
        return result;
    }

    result.outputWidth = output.width();
    result.outputHeight = output.height();
    result.image = output;
    result.ok = true;
    return result;
}

} // namespace

PhotoOverlayController::PhotoOverlayController(QObject* parent)
    : QObject(parent)
    , _settings(QSettings::IniFormat, QSettings::UserScope,
                QCoreApplication::organizationName(), QCoreApplication::applicationName())
{
    _loadSettings();
    _watcher = new QFutureWatcher<PhotoOverlayResult>(this);
    connect(_watcher, &QFutureWatcher<PhotoOverlayResult>::finished,
            this, &PhotoOverlayController::_onFinished);
}

PhotoOverlayController::~PhotoOverlayController()
{
    if (_cancel) _cancel->store(true);
}

void PhotoOverlayController::_loadSettings()
{
    _settings.beginGroup(QStringLiteral("MagLab/PhotoOverlay"));
    _hfov = qBound(1.0, _settings.value(QStringLiteral("hfov"), _hfov).toDouble(), 179.0);
    _vfov = qBound(1.0, _settings.value(QStringLiteral("vfov"), _vfov).toDouble(), 179.0);
    _heightAGL = qBound(0.1, _settings.value(QStringLiteral("heightAGL"), _heightAGL).toDouble(), 10000.0);
    _yawOffset = _settings.value(QStringLiteral("yawOffset"), _yawOffset).toDouble();
    _transparency = qBound(0.0, _settings.value(QStringLiteral("transparency"), _transparency).toDouble(), 1.0);
    _settings.endGroup();
}

void PhotoOverlayController::_saveSettings(const QString& key, const QVariant& value)
{
    _settings.beginGroup(QStringLiteral("MagLab/PhotoOverlay"));
    _settings.setValue(key, value);
    _settings.endGroup();
    _settings.sync();
    emit settingsChanged();
}

void PhotoOverlayController::setHfov(double value)
{
    value = qBound(1.0, value, 179.0);
    if (qFuzzyCompare(_hfov + 1.0, value + 1.0)) return;
    _hfov = value;
    _saveSettings(QStringLiteral("hfov"), value);
}

void PhotoOverlayController::setVfov(double value)
{
    value = qBound(1.0, value, 179.0);
    if (qFuzzyCompare(_vfov + 1.0, value + 1.0)) return;
    _vfov = value;
    _saveSettings(QStringLiteral("vfov"), value);
}

void PhotoOverlayController::setHeightAGL(double value)
{
    value = qBound(0.1, value, 10000.0);
    if (qFuzzyCompare(_heightAGL + 1.0, value + 1.0)) return;
    _heightAGL = value;
    _saveSettings(QStringLiteral("heightAGL"), value);
}

void PhotoOverlayController::setYawOffset(double value)
{
    if (qFuzzyCompare(_yawOffset + 1.0, value + 1.0)) return;
    _yawOffset = value;
    _saveSettings(QStringLiteral("yawOffset"), value);
}

void PhotoOverlayController::setTransparency(double value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(_transparency + 1.0, value + 1.0)) return;
    _transparency = value;
    _saveSettings(QStringLiteral("transparency"), value);
}

void PhotoOverlayController::_setBusy(bool busy)
{
    if (_busy == busy) return;
    _busy = busy;
    emit busyChanged();
}

void PhotoOverlayController::_setError(const QString& error)
{
    if (_errorString == error) return;
    _errorString = error;
    emit errorStringChanged();
}

void PhotoOverlayController::_setStatus(const QString& status)
{
    if (_statusText == status) return;
    _statusText = status;
    emit statusTextChanged();
}

QString PhotoOverlayController::sidecarPath(const QString& photoPath) const
{
    return sidecarFor(photoPath);
}

bool PhotoOverlayController::importPoseSidecar(const QString& photoPath, const QString& sidecarPath)
{
    if (photoPath.isEmpty() || sidecarPath.isEmpty() || !QFileInfo::exists(sidecarPath)) {
        _setError(tr("照片或位姿文件不存在"));
        return false;
    }
    const QString target = sidecarFor(photoPath);
    QDir().mkpath(QFileInfo(target).absolutePath());
    if (QFileInfo(sidecarPath).absoluteFilePath() == QFileInfo(target).absoluteFilePath()) {
        return true;
    }
    QFile::remove(target);
    if (!QFile::copy(sidecarPath, target)) {
        _setError(tr("无法复制位姿文件到照片目录"));
        return false;
    }
    _setError({});
    _setStatus(tr("已导入照片位姿：%1").arg(QFileInfo(target).fileName()));
    return true;
}

bool PhotoOverlayController::hasPhotoSidecar(const QString& photoPath) const
{
    return !photoPath.isEmpty() &&
            (QFileInfo::exists(sidecarFor(photoPath)) ||
             QFileInfo::exists(legacyAdjacentSidecar(photoPath)));
}

bool PhotoOverlayController::hasMagSidecar(const QString& magImagePath) const
{
    if (magImagePath.isEmpty()) return false;
    const QFileInfo info(magImagePath);
    return QFileInfo::exists(info.dir().filePath(
                QStringLiteral("txt%1.txt").arg(info.completeBaseName())));
}

QString PhotoOverlayController::writeSidecar(const QString& photoPath, double lon, double lat,
                                             double altitude, double roll, double pitch, double yaw)
{
    if (photoPath.isEmpty() || !std::isfinite(lon) || !std::isfinite(lat) ||
        !std::isfinite(altitude) || !std::isfinite(roll) ||
        !std::isfinite(pitch) || !std::isfinite(yaw)) {
        _setError(tr("照片位姿无效，未写入 sidecar"));
        return {};
    }
    const QString path = sidecarFor(photoPath);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        _setError(tr("无法创建照片位姿文件：%1").arg(path));
        return {};
    }
    QTextStream out(&file);
    out.setRealNumberNotation(QTextStream::FixedNotation);
    out.setRealNumberPrecision(8);
    out << lon << "," << lat << "\n";
    out << roll << "," << pitch << "," << yaw << "\n";
    out << altitude << "\n";
    if (std::isfinite(_heightAGL) && _heightAGL > 0.0) {
        out << _heightAGL << "\n";
    }
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        _setError(tr("照片位姿文件写入失败：%1").arg(path));
        return {};
    }
    _setError({});
    _setStatus(tr("已记录照片位姿：%1").arg(QFileInfo(path).fileName()));
    return path;
}

QString PhotoOverlayController::saveOutput()
{
    if (_previewImage.isNull()) {
        _setError(tr("没有可保存的叠加预览"));
        return {};
    }
    const QString outDir = MagLabPaths::instance()->photoOverlayDir();
    if (outDir.isEmpty()) {
        _setError(tr("无法创建照片叠加输出目录"));
        return {};
    }
    QDir().mkpath(outDir);
    const QString sessionName = MissionArchiveManager::instance()->currentSessionName();
    const QString path = QDir(outDir).filePath(
                QStringLiteral("%1_overlay_%2.jpg")
                    .arg(sessionName, QUuid::createUuid().toString(QUuid::Id128)));
    if (!_previewImage.save(path, "JPG", 92)) {
        _setError(tr("叠加图片保存失败：%1").arg(path));
        return {};
    }
    _outputPath = path;
    emit outputPathChanged();
    _setError({});
    _setStatus(tr("已保存叠加图：%1").arg(QFileInfo(path).fileName()));
    return path;
}

double PhotoOverlayController::readSurveyAgl(const QString& surveyPath) const
{
    QFile file(surveyPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return std::numeric_limits<double>::quiet_NaN();
    QTextStream stream(&file);
    const QString header = stream.readLine();
    const QStringList names = header.split(QLatin1Char(','));
    int aglColumn = -1;
    for (int i = 0; i < names.size(); ++i) {
        const QString name = names.at(i).trimmed().toLower();
        if (name.contains(QStringLiteral("离地")) || name.contains(QStringLiteral("agl")) ||
            name.contains(QStringLiteral("lidar")) || name.contains(QStringLiteral("对地"))) {
            aglColumn = i;
            break;
        }
    }
    if (aglColumn < 0) return std::numeric_limits<double>::quiet_NaN();
    while (!stream.atEnd()) {
        const QStringList fields = stream.readLine().split(QLatin1Char(','));
        if (fields.size() <= aglColumn) continue;
        bool ok = false;
        const double value = fields.at(aglColumn).trimmed().toDouble(&ok);
        if (ok && std::isfinite(value) && value > 0.0 && value < 10000.0) return value;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

QVariantMap PhotoOverlayController::loadBundle(const QString& manifestPath)
{
    QVariantMap result;
    result[QStringLiteral("ok")] = false;
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        result[QStringLiteral("error")] = tr("无法读取任务包：%1").arg(manifestPath);
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result[QStringLiteral("error")] = tr("任务包 manifest.json 格式错误");
        return result;
    }
    const QJsonObject object = document.object();
    const QFileInfo manifestInfo(manifestPath);
    const QDir base = manifestInfo.dir();
    auto resolve = [&base](const QJsonValue& value) {
        if (!value.isString()) return QString();
        const QString path = value.toString();
        return QFileInfo(path).isAbsolute() ? path : base.filePath(path);
    };
    const QString photo = resolve(object.value(QStringLiteral("photo")));
    if (photo.isEmpty() || !QFileInfo::exists(photo)) {
        result[QStringLiteral("error")] = tr("任务包缺少有效照片文件");
        return result;
    }
    const QJsonObject mapObject = object.value(QStringLiteral("map")).toObject();
    QString mapPath = resolve(mapObject.value(QStringLiteral("file")));
    if (mapPath.isEmpty()) {
        // Also accept a compact top-level "map" string.
        mapPath = resolve(object.value(QStringLiteral("map")));
    }
    const QString posePath = resolve(object.value(QStringLiteral("pose")));
    const QString surveyPath = resolve(object.value(QStringLiteral("survey")));
    if (!posePath.isEmpty() && !importPoseSidecar(photo, posePath)) {
        result[QStringLiteral("error")] = _errorString;
        return result;
    }
    if (!mapPath.isEmpty() && !QFileInfo::exists(mapPath)) {
        result[QStringLiteral("error")] = tr("任务包中的磁图不存在：%1").arg(mapPath);
        return result;
    }
    // A task package may carry the magnetic-map corner file separately from
    // the legacy `txt<mapBase>.txt` naming convention. Copy it into the
    // conventional location so the renderer can consume both formats.
    const QString cornersPath = resolve(mapObject.value(QStringLiteral("corners")));
    if (!mapPath.isEmpty() && !cornersPath.isEmpty() && QFileInfo::exists(cornersPath)) {
        const QFileInfo mapInfo(mapPath);
        const QString expected = mapInfo.dir().filePath(
                    QStringLiteral("txt%1.txt").arg(mapInfo.completeBaseName()));
        if (!QFileInfo::exists(expected)) {
            QFile::copy(cornersPath, expected);
        }
    }
    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("photo")] = photo;
    result[QStringLiteral("map")] = mapPath;
    result[QStringLiteral("survey")] = surveyPath;
    const bool hasRange = mapObject.contains(QStringLiteral("minValue")) &&
                          mapObject.contains(QStringLiteral("maxValue"));
    result[QStringLiteral("hasRange")] = hasRange;
    if (hasRange) {
        result[QStringLiteral("minValue")] = mapObject.value(QStringLiteral("minValue")).toDouble();
        result[QStringLiteral("maxValue")] = mapObject.value(QStringLiteral("maxValue")).toDouble();
    }
    return result;
}

QString PhotoOverlayController::findAssociatedMap(const QString& photoPath) const
{
    if (photoPath.isEmpty()) return {};
    QString photoBase = QFileInfo(photoPath).completeBaseName();
    const int photoMarker = photoBase.indexOf(QStringLiteral("_photo_"));
    if (photoMarker >= 0) photoBase = photoBase.left(photoMarker);
    for (const QString& mapPath : MagLabPaths::instance()->magMapFiles()) {
        QString mapBase = QFileInfo(mapPath).completeBaseName();
        const int mapMarker = mapBase.indexOf(QStringLiteral("_map_"));
        if (mapMarker >= 0) mapBase = mapBase.left(mapMarker);
        if (!mapBase.isEmpty() && mapBase == photoBase && hasMagSidecar(mapPath)) {
            return mapPath;
        }
    }
    return {};
}

QVariantMap PhotoOverlayController::pixelToGeo(const QString& photoPath, double x, double y) const
{
    QVariantMap result;
    result[QStringLiteral("valid")] = false;
    QImage photo(photoPath);
    if (photo.isNull()) return result;
    const QString sidecar = QFileInfo::exists(sidecarFor(photoPath))
            ? sidecarFor(photoPath) : legacyAdjacentSidecar(photoPath);
    PhotoPose pose;
    QString error;
    if (!readPose(sidecar, pose, error)) return result;
    // Output layout used by buildOverlay().
    constexpr int left = 140;
    constexpr int top = 62;
    const double u = x - left;
    const double v = y - top;
    if (u < 0.0 || v < 0.0 || u > photo.width() - 1 || v > photo.height() - 1) return result;
    GeoCameraProjector::CameraIntrinsics K =
        GeoCameraProjector::intrinsicsFromFov(photo.width(), photo.height(), _hfov, _vfov);
    GeoCameraProjector::CameraPose cameraPose;
    cameraPose.refLatDeg = pose.lat;
    cameraPose.refLonDeg = pose.lon;
    cameraPose.refAltM = 0.0;
    cameraPose.heightAGL = _heightAGL;
    cameraPose.rollDeg = pose.roll;
    cameraPose.pitchDeg = pose.pitch;
    cameraPose.yawDeg = pose.yaw + _yawOffset;
    GeoCameraProjector::GeoPoint geo;
    if (!GeoCameraProjector::pixelToGeoByPose(u, v, 0.0, cameraPose, K, geo)) return result;
    result[QStringLiteral("valid")] = true;
    result[QStringLiteral("lon")] = geo.lonDeg;
    result[QStringLiteral("lat")] = geo.latDeg;
    return result;
}

QVariantMap PhotoOverlayController::mapPixelToGeo(const QString& magImagePath, double x, double y) const
{
    QVariantMap result;
    result[QStringLiteral("valid")] = false;
    QImage mag(magImagePath);
    if (mag.isNull() || mag.width() < 2 || mag.height() < 2) return result;
    const QFileInfo info(magImagePath);
    const QString sidecar = info.dir().filePath(QStringLiteral("txt%1.txt").arg(info.completeBaseName()));
    Corner corners[4];
    double minValue = std::numeric_limits<double>::quiet_NaN();
    double maxValue = minValue;
    QString error;
    if (!readCorners(sidecar, corners, minValue, maxValue, error)) return result;
    const double u = qBound(0.0, x / double(mag.width() - 1), 1.0);
    const double v = qBound(0.0, y / double(mag.height() - 1), 1.0);
    // Corners are NW, SW, SE, NE.
    const double topLon = corners[0].lon + (corners[3].lon - corners[0].lon) * u;
    const double bottomLon = corners[1].lon + (corners[2].lon - corners[1].lon) * u;
    const double leftLat = corners[0].lat + (corners[1].lat - corners[0].lat) * v;
    const double rightLat = corners[3].lat + (corners[2].lat - corners[3].lat) * v;
    result[QStringLiteral("valid")] = true;
    result[QStringLiteral("lon")] = topLon + (bottomLon - topLon) * v;
    result[QStringLiteral("lat")] = leftLat + (rightLat - leftLat) * u;
    return result;
}

void PhotoOverlayController::generate(const QString& photoPath, const QString& magImagePath,
                                      double minValue, double maxValue)
{
    if (_busy) return;
    if (photoPath.isEmpty() || magImagePath.isEmpty()) {
        _setError(tr("请选择照片和磁图"));
        return;
    }
    const QString outputPath = QDir(QDir::tempPath()).filePath(
                QStringLiteral("maglab-overlay-preview-%1.jpg").arg(QUuid::createUuid().toString(QUuid::Id128)));

    _previewImage = QImage();
    _previewPath.clear();
    _outputPath.clear();
    emit previewPathChanged();
    emit outputPathChanged();
    _setBusy(true);
    _setError({});
    _setStatus(tr("正在投影并叠加磁图…"));
    _cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = _cancel;
    const double hfov = _hfov;
    const double vfov = _vfov;
    const double heightAGL = _heightAGL;
    const double yawOffset = _yawOffset;
    const double transparency = _transparency;
    _watcher->setFuture(QtConcurrent::run([=] {
        return buildOverlay(photoPath, magImagePath, hfov, vfov, heightAGL,
                            yawOffset, transparency, minValue, maxValue, outputPath, cancel);
    }));
}

void PhotoOverlayController::cancel()
{
    if (_cancel) _cancel->store(true);
}

void PhotoOverlayController::_onFinished()
{
    const PhotoOverlayResult result = _watcher->result();
    _setBusy(false);
    if (result.cancelled) {
        _setStatus(tr("已取消照片叠加"));
        emit finished(false);
        return;
    }
    if (!result.ok) {
        _setError(result.error);
        _setStatus(tr("照片叠加失败"));
        emit finished(false);
        return;
    }
    _previewImage = result.image;
    _previewPath = result.outputPath;
    _previewImage.save(_previewPath, "JPG", 92);
    _outputWidth = result.outputWidth;
    _outputHeight = result.outputHeight;
    emit previewPathChanged();
    emit outputSizeChanged();
    _setError({});
    _setStatus(tr("已保存叠加图：%1").arg(QFileInfo(result.outputPath).fileName()));
    emit finished(true);
}
