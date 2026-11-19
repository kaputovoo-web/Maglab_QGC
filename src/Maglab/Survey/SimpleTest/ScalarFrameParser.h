#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cmath>
#include <limits>

namespace ScalarFrameParser {

/// Scalar protocol (29 '+'-separated fields):
/// UU + len + time + lon + lat + alt_msl + fix_type + lidar +
/// op1 + op2 + mx + my + mz + roll + pitch + yaw +
/// satellites + barometer + column1 + column2 + column3 +
/// gyroX/Y/Z + accX/Y/Z + count + CRC16
///
/// The legacy 24-field form (UU + id + time..accZ) is accepted as a fallback
/// so old recordings and older probe firmware remain readable.
struct Frame {
    double time = std::numeric_limits<double>::quiet_NaN();
    double lon = std::numeric_limits<double>::quiet_NaN();
    double lat = std::numeric_limits<double>::quiet_NaN();
    double altitude = std::numeric_limits<double>::quiet_NaN();
    int fixType = 0;
    double lidar = std::numeric_limits<double>::quiet_NaN();
    double op1 = std::numeric_limits<double>::quiet_NaN();
    double op2 = std::numeric_limits<double>::quiet_NaN();
    double mx = std::numeric_limits<double>::quiet_NaN();
    double my = std::numeric_limits<double>::quiet_NaN();
    double mz = std::numeric_limits<double>::quiet_NaN();
    double roll = std::numeric_limits<double>::quiet_NaN();
    double pitch = std::numeric_limits<double>::quiet_NaN();
    double yaw = std::numeric_limits<double>::quiet_NaN();
    double satellites = 0.0;
    double barometer = std::numeric_limits<double>::quiet_NaN();
    double column1 = std::numeric_limits<double>::quiet_NaN();
    double column2 = std::numeric_limits<double>::quiet_NaN();
    double column3 = std::numeric_limits<double>::quiet_NaN();
    double gyroX = std::numeric_limits<double>::quiet_NaN();
    double gyroY = std::numeric_limits<double>::quiet_NaN();
    double gyroZ = std::numeric_limits<double>::quiet_NaN();
    double accX = std::numeric_limits<double>::quiet_NaN();
    double accY = std::numeric_limits<double>::quiet_NaN();
    double accZ = std::numeric_limits<double>::quiet_NaN();
    quint32 count = 0;
    QString crc;
};

inline bool numberAt(const QStringList& fields, int index, double& value)
{
    bool ok = false;
    value = fields.at(index).trimmed().toDouble(&ok);
    return ok && std::isfinite(value);
}

inline bool parse(const QByteArray& frame, Frame& out)
{
    const QStringList fields = QString::fromUtf8(frame).trimmed().split(
                QLatin1Char('+'), Qt::KeepEmptyParts);
    if (fields.isEmpty() || fields.at(0) != QStringLiteral("UU")) {
        return false;
    }

    Frame parsed;
    bool ok = false;

    if (fields.size() == 29) {
        // Current scalar protocol.
        if (!numberAt(fields, 2, parsed.time) ||
            !numberAt(fields, 3, parsed.lon) ||
            !numberAt(fields, 4, parsed.lat) ||
            !numberAt(fields, 5, parsed.altitude)) {
            return false;
        }
        parsed.fixType = fields.at(6).trimmed().toInt(&ok);
        if (!ok || !numberAt(fields, 7, parsed.lidar) ||
            !numberAt(fields, 8, parsed.op1) ||
            !numberAt(fields, 9, parsed.op2) ||
            !numberAt(fields, 10, parsed.mx) ||
            !numberAt(fields, 11, parsed.my) ||
            !numberAt(fields, 12, parsed.mz) ||
            !numberAt(fields, 13, parsed.roll) ||
            !numberAt(fields, 14, parsed.pitch) ||
            !numberAt(fields, 15, parsed.yaw) ||
            !numberAt(fields, 16, parsed.satellites) ||
            !numberAt(fields, 17, parsed.barometer) ||
            !numberAt(fields, 18, parsed.column1) ||
            !numberAt(fields, 19, parsed.column2) ||
            !numberAt(fields, 20, parsed.column3) ||
            !numberAt(fields, 21, parsed.gyroX) ||
            !numberAt(fields, 22, parsed.gyroY) ||
            !numberAt(fields, 23, parsed.gyroZ) ||
            !numberAt(fields, 24, parsed.accX) ||
            !numberAt(fields, 25, parsed.accY) ||
            !numberAt(fields, 26, parsed.accZ)) {
            return false;
        }
        parsed.count = fields.at(27).trimmed().toUInt(&ok);
        if (!ok) return false;
        parsed.crc = fields.at(28);
    } else if (fields.size() == 24) {
        // Legacy scalar protocol: UU + id + time..accZ.
        parsed.fixType = fields.at(6).trimmed().toInt(&ok);
        if (!ok || !numberAt(fields, 2, parsed.time) ||
            !numberAt(fields, 3, parsed.lon) ||
            !numberAt(fields, 4, parsed.lat) ||
            !numberAt(fields, 5, parsed.altitude) ||
            !numberAt(fields, 7, parsed.lidar) ||
            !numberAt(fields, 8, parsed.op1) ||
            !numberAt(fields, 9, parsed.op2) ||
            !numberAt(fields, 10, parsed.mx) ||
            !numberAt(fields, 11, parsed.my) ||
            !numberAt(fields, 12, parsed.mz) ||
            !numberAt(fields, 13, parsed.roll) ||
            !numberAt(fields, 14, parsed.pitch) ||
            !numberAt(fields, 15, parsed.yaw) ||
            !numberAt(fields, 16, parsed.satellites) ||
            !numberAt(fields, 17, parsed.barometer) ||
            !numberAt(fields, 18, parsed.gyroX) ||
            !numberAt(fields, 19, parsed.gyroY) ||
            !numberAt(fields, 20, parsed.gyroZ) ||
            !numberAt(fields, 21, parsed.accX) ||
            !numberAt(fields, 22, parsed.accY) ||
            !numberAt(fields, 23, parsed.accZ)) {
            return false;
        }
    } else {
        return false;
    }

    if (parsed.lon < -180.0 || parsed.lon > 180.0 ||
        parsed.lat < -90.0 || parsed.lat > 90.0) {
        return false;
    }
    out = parsed;
    return true;
}

} // namespace ScalarFrameParser
