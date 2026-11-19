#pragma once

#include <QByteArray>
#include <QStringList>
#include <QString>
#include <cmath>
#include <limits>

namespace VectorFrameParser {

struct Frame {
    double time = std::numeric_limits<double>::quiet_NaN();
    double lon = std::numeric_limits<double>::quiet_NaN();
    double lat = std::numeric_limits<double>::quiet_NaN();
    double altitude = std::numeric_limits<double>::quiet_NaN();
    int fixType = 0;
    double lidar = std::numeric_limits<double>::quiet_NaN();
    double fg1x = std::numeric_limits<double>::quiet_NaN();
    double fg1y = std::numeric_limits<double>::quiet_NaN();
    double fg1z = std::numeric_limits<double>::quiet_NaN();
    double fg2x = std::numeric_limits<double>::quiet_NaN();
    double fg2y = std::numeric_limits<double>::quiet_NaN();
    double fg2z = std::numeric_limits<double>::quiet_NaN();
    double gyroX = std::numeric_limits<double>::quiet_NaN();
    double gyroY = std::numeric_limits<double>::quiet_NaN();
    double gyroZ = std::numeric_limits<double>::quiet_NaN();
    double accX = std::numeric_limits<double>::quiet_NaN();
    double accY = std::numeric_limits<double>::quiet_NaN();
    double accZ = std::numeric_limits<double>::quiet_NaN();
    double roll = std::numeric_limits<double>::quiet_NaN();
    double pitch = std::numeric_limits<double>::quiet_NaN();
    double yaw = std::numeric_limits<double>::quiet_NaN();
    quint32 count = 0;
    QString crc;
};

inline bool parse(const QByteArray& frame, Frame& out)
{
    const QString text = QString::fromUtf8(frame).trimmed();
    if (!text.startsWith(QStringLiteral("UU"))) {
        return false;
    }
    const QStringList fields = text.split(QLatin1Char('+'), Qt::KeepEmptyParts);
    // The vector protocol is exactly 25 fields:
    // UU + len + time..yaw + count + CRC16. Scalar frames use 29 fields and
    // must not be accepted by the vector parser.
    if (fields.size() != 25) {
        return false;
    }
    bool ok = false;
    Frame parsed;
    // fields[0] = UU, fields[1] = length; len is intentionally not trusted.
    parsed.time = fields[2].toDouble(&ok);
    if (!ok) return false;
    parsed.lon = fields[3].toDouble(&ok);
    if (!ok) return false;
    parsed.lat = fields[4].toDouble(&ok);
    if (!ok) return false;
    parsed.altitude = fields[5].toDouble(&ok);
    if (!ok) return false;
    parsed.fixType = fields[6].toInt(&ok);
    if (!ok) return false;
    parsed.lidar = fields[7].toDouble(&ok);
    if (!ok) return false;
    parsed.fg1x = fields[8].toDouble(&ok); if (!ok) return false;
    parsed.fg1y = fields[9].toDouble(&ok); if (!ok) return false;
    parsed.fg1z = fields[10].toDouble(&ok); if (!ok) return false;
    parsed.fg2x = fields[11].toDouble(&ok); if (!ok) return false;
    parsed.fg2y = fields[12].toDouble(&ok); if (!ok) return false;
    parsed.fg2z = fields[13].toDouble(&ok); if (!ok) return false;
    parsed.gyroX = fields[14].toDouble(&ok); if (!ok) return false;
    parsed.gyroY = fields[15].toDouble(&ok); if (!ok) return false;
    parsed.gyroZ = fields[16].toDouble(&ok); if (!ok) return false;
    parsed.accX = fields[17].toDouble(&ok); if (!ok) return false;
    parsed.accY = fields[18].toDouble(&ok); if (!ok) return false;
    parsed.accZ = fields[19].toDouble(&ok); if (!ok) return false;
    parsed.roll = fields[20].toDouble(&ok); if (!ok) return false;
    parsed.pitch = fields[21].toDouble(&ok); if (!ok) return false;
    parsed.yaw = fields[22].toDouble(&ok); if (!ok) return false;
    parsed.count = fields[23].toUInt(&ok); if (!ok) return false;
    parsed.crc = fields[24];

    if (!std::isfinite(parsed.lon) || !std::isfinite(parsed.lat) ||
        parsed.lon < -180.0 || parsed.lon > 180.0 ||
        parsed.lat < -90.0 || parsed.lat > 90.0) {
        return false;
    }
    out = parsed;
    return true;
}

} // namespace VectorFrameParser
