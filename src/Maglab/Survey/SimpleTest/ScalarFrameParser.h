#pragma once

#include <QByteArray>
#include <QList>

#include <array>
#include <cmath>

namespace ScalarFrameParser {

/// Fixed legacy protocol: UU + integer ID + 22 numeric fields. Preserve empty
/// columns so missing measurements can never shift subsequent sensor values.
inline bool parse(const QByteArray& frame, std::array<double, 22>& values)
{
    const QList<QByteArray> fields = frame.trimmed().split('+');
    if (fields.size() != 24 || fields.first() != "UU") {
        return false;
    }
    bool ok = false;
    fields[1].toInt(&ok);
    if (!ok) {
        return false;
    }
    std::array<double, 22> parsed{};
    for (size_t i = 0; i < parsed.size(); ++i) {
        parsed[i] = fields[static_cast<qsizetype>(i) + 2].toDouble(&ok);
        if (!ok || !std::isfinite(parsed[i])) {
            return false;
        }
    }
    if (parsed[1] < -180.0 || parsed[1] > 180.0 || parsed[2] < -90.0 || parsed[2] > 90.0) {
        return false;
    }
    values = parsed;
    return true;
}

} // namespace ScalarFrameParser
