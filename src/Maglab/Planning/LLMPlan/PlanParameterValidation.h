#pragma once

#include <QJsonObject>
#include <QString>

#include <cmath>
#include <initializer_list>
#include <limits>

namespace PlanParameterValidation {

struct NumberRule {
    const char* key;
    double minimum = -std::numeric_limits<double>::max();
    double maximum = std::numeric_limits<double>::max();
    bool integer = false;
};

/// Validate optional numeric fields before any Facts are changed. Missing fields
/// keep their current value; present fields must never be silently coerced.
inline bool validate(const QJsonObject& parameters, std::initializer_list<NumberRule> rules)
{
    for (const NumberRule& rule : rules) {
        const QJsonValue value = parameters.value(QLatin1String(rule.key));
        if (value.isUndefined()) {
            continue;
        }
        if (!value.isDouble()) {
            return false;
        }
        const double number = value.toDouble();
        if (!std::isfinite(number) || number < rule.minimum || number > rule.maximum ||
                (rule.integer && std::trunc(number) != number)) {
            return false;
        }
    }
    return true;
}

inline bool spiral(const QJsonObject& parameters)
{
    return validate(parameters, {
        {"startRadius", 0.0, 1000.0}, {"spacing", 1.0, 500.0}, {"turns", 0.5, 50.0},
        {"altitude", 0.0, 400.0}, {"direction", 0.0, 1.0, true},
        {"flightSpeed", 1.0, 30.0}, {"altitudeMode", 0.0, 2.0, true},
        {"maxBankAngle", 5.0, 35.0}, {"rotationAngle", 0.0, 360.0}
    });
}

inline bool square(const QJsonObject& parameters)
{
    return validate(parameters, {
        {"spacing", 1.0, 500.0}, {"legs", 2.0, 60.0, true}, {"altitude", 0.0, 400.0},
        {"direction", 0.0, 1.0, true}, {"flightSpeed", 1.0, 30.0},
        {"altitudeMode", 0.0, 2.0, true}, {"rotationAngle", 0.0, 360.0}
    });
}

inline bool fom(const QJsonObject& parameters)
{
    return validate(parameters, {
        {"sideLength", 5.0, 500.0}, {"radius", 2.5, 250.0},
        {"flightSpeed", 1.0, 30.0}, {"altitude", 0.0, 400.0},
        {"altitudeMode", 0.0, 2.0, true}, {"cycles", 1.0, 10.0, true},
        {"loops", 1.0, 10.0, true}, {"rollAmp", 0.0, 60.0}, {"rollLimit", 0.0, 60.0},
        {"pitchAmp", 0.0, 60.0}, {"pitchLimit", 0.0, 60.0},
        {"yawAmp", 0.0, 60.0}, {"yawLimit", 0.0, 60.0}
    });
}

} // namespace PlanParameterValidation
