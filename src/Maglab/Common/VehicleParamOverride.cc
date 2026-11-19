#include "VehicleParamOverride.h"

#include "Vehicle.h"
#include "MultiVehicleManager.h"
#include "MissionManager.h"
#include "MissionItem.h"
#include "ParameterManager.h"
#include "Fact.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <cmath>

QGC_LOGGING_CATEGORY(VehicleParamOverrideLog, "VehicleParamOverrideLog")

namespace {
// Override targets while a spiral mission is flying. Values are in the
// parameter's native units: WPNAV_ACCEL is cm/s² (400 => 4.0 m/s²) and
// WPNAV_JERK is m/s³ (10). Both stay well inside a multi-rotor's safe envelope
// (~22° bank for 4 m/s²) and are restored on Auto-exit / disarm.
constexpr double kOverrideAccel = 400.0;  // cm/s²
constexpr double kOverrideJerk  = 10.0;   // m/s³

// ArduPilot renamed the waypoint navigation parameters from WP_* to WPNAV_* in
// Copter 3.3, and only added WPNAV_JERK in Copter 4.1. Resolve the name that
// actually exists on this vehicle, preferring the modern name.
QString resolveParamName(ParameterManager* pm, int compId,
                         const QString& modernName, const QString& legacyName)
{
    if (pm->parameterExists(compId, modernName)) {
        return modernName;
    }
    if (pm->parameterExists(compId, legacyName)) {
        return legacyName;
    }
    return QString();
}

const QString kAccelModern = QStringLiteral("WPNAV_ACCEL");
const QString kAccelLegacy = QStringLiteral("WP_ACCEL");
const QString kJerkModern  = QStringLiteral("WPNAV_JERK");
const QString kJerkLegacy  = QStringLiteral("WP_JERK");
const QString kAutoMode    = QStringLiteral("Auto");
}  // namespace

VehicleParamOverride* VehicleParamOverride::s_instance = nullptr;

VehicleParamOverride::VehicleParamOverride(QObject* parent)
    : QObject(parent)
{
}

VehicleParamOverride* VehicleParamOverride::instance()
{
    if (s_instance == nullptr) {
        s_instance = new VehicleParamOverride();
    }
    return s_instance;
}

void VehicleParamOverride::init(MultiVehicleManager* multiVehicleManager)
{
    if (_initialized || multiVehicleManager == nullptr) {
        return;
    }
    _initialized = true;

    connect(multiVehicleManager, &MultiVehicleManager::activeVehicleChanged,
            this, &VehicleParamOverride::_setActiveVehicle);
    _setActiveVehicle(multiVehicleManager->activeVehicle());
}

void VehicleParamOverride::_setActiveVehicle(Vehicle* vehicle)
{
    if (vehicle == _vehicle) {
        return;
    }

    // Leaving a vehicle while still overridden: restore before detaching so we
    // never abandon a modified vehicle.
    if (_applied && _vehicle != nullptr) {
        _restore();
    }

    if (_vehicle != nullptr) {
        disconnect(_vehicle, nullptr, this, nullptr);
        if (_vehicle->parameterManager() != nullptr) {
            disconnect(_vehicle->parameterManager(), nullptr, this, nullptr);
        }
    }

    _vehicle = vehicle;
    _applied = false;

    if (_vehicle == nullptr) {
        return;
    }

    connect(_vehicle, &Vehicle::flightModeChanged, this, &VehicleParamOverride::_onFlightModeChanged);
    connect(_vehicle, &Vehicle::armedChanged, this, &VehicleParamOverride::_onArmedChanged);
    connect(_vehicle->parameterManager(), &ParameterManager::parametersReadyChanged,
            this, &VehicleParamOverride::_onParametersReady);

    // Recover any override left stranded by a previous crash/relink.
    if (_vehicle->parameterManager()->parametersReady()) {
        _recoverStaleJournal(_vehicle);
    }
}

void VehicleParamOverride::_onParametersReady(bool ready)
{
    if (ready && _vehicle != nullptr) {
        _recoverStaleJournal(_vehicle);
    }
}

void VehicleParamOverride::_onFlightModeChanged(const QString& mode)
{
    if (_vehicle == nullptr) {
        return;
    }

    if (mode == kAutoMode) {
        if (!_applied && _missionContainsSpiral()) {
            _apply();
        }
    } else if (_applied) {
        _restore();
    }
}

void VehicleParamOverride::_onArmedChanged(bool armed)
{
    if (!armed && _applied && _vehicle != nullptr) {
        _restore();
    }
}

bool VehicleParamOverride::_missionContainsSpiral() const
{
    if (_vehicle == nullptr || _vehicle->missionManager() == nullptr) {
        return false;
    }

    // The spiral is the only MagLab pattern that emits NAV_SPLINE_WAYPOINT;
    // the comb (native Survey) uses NAV_WAYPOINT. This is the pragmatic
    // "is this mission a spiral" probe.
    const QList<MissionItem*>& items = _vehicle->missionManager()->missionItems();
    for (const MissionItem* item : items) {
        if (item != nullptr && item->command() == MAV_CMD_NAV_SPLINE_WAYPOINT) {
            return true;
        }
    }
    return false;
}

bool VehicleParamOverride::_apply()
{
    if (_vehicle == nullptr) {
        return false;
    }

    ParameterManager* pm = _vehicle->parameterManager();
    const int compId = _vehicle->defaultComponentId();

    // Accel is the primary override and is required; jerk is optional because
    // WPNAV_JERK only exists on Copter 4.1+ (and may be absent on older builds).
    const QString accelName = resolveParamName(pm, compId, kAccelModern, kAccelLegacy);
    const QString jerkName  = resolveParamName(pm, compId, kJerkModern, kJerkLegacy);

    if (accelName.isEmpty()) {
        emit warning(tr("未找到航点加速度参数（WPNAV_ACCEL / WP_ACCEL），螺旋加速优化未生效。"));
        return false;
    }

    Fact* accelFact = pm->getParameter(compId, accelName);
    if (accelFact == nullptr) {
        emit warning(tr("无法读取航点加速度参数，螺旋加速优化未生效。"));
        return false;
    }

    const double origAccel = accelFact->rawValue().toDouble();
    if (!std::isfinite(origAccel) || origAccel <= 0.0) {
        emit warning(tr("航点加速度参数值异常，已跳过螺旋加速优化。"));
        return false;
    }

    double origJerk = 0.0;
    Fact* jerkFact = nullptr;
    if (!jerkName.isEmpty()) {
        jerkFact = pm->getParameter(compId, jerkName);
        if (jerkFact != nullptr) {
            const double value = jerkFact->rawValue().toDouble();
            if (std::isfinite(value) && value > 0.0) {
                origJerk = value;
            } else {
                jerkFact = nullptr;  // unusable value -> treat as not present
            }
        }
    }

    _accelName = accelName;
    _jerkName  = jerkFact != nullptr ? jerkName : QString();
    _origAccel = origAccel;
    _origJerk  = origJerk;

    accelFact->setRawValue(kOverrideAccel);
    if (jerkFact != nullptr) {
        jerkFact->setRawValue(kOverrideJerk);
    }

    _writeJournal(_vehicle->id());
    _applied = true;

    qCInfo(VehicleParamOverrideLog) << "Spiral override applied:" << accelName
                                    << origAccel << "->" << kOverrideAccel;
    if (jerkFact != nullptr) {
        qCInfo(VehicleParamOverrideLog) << "Spiral override applied:" << jerkName
                                        << origJerk << "->" << kOverrideJerk;
    } else {
        qCInfo(VehicleParamOverrideLog) << "No jerk parameter on this vehicle; skipping jerk override";
    }
    return true;
}

bool VehicleParamOverride::_restore()
{
    if (_vehicle == nullptr || !_applied) {
        return false;
    }

    ParameterManager* pm = _vehicle->parameterManager();
    const int compId = _vehicle->defaultComponentId();

    Fact* accelFact = pm->getParameter(compId, _accelName);
    if (accelFact == nullptr) {
        emit warning(tr("航点参数恢复失败：无法访问参数。"));
        return false;
    }
    accelFact->setRawValue(_origAccel);

    if (!_jerkName.isEmpty()) {
        Fact* jerkFact = pm->getParameter(compId, _jerkName);
        if (jerkFact != nullptr) {
            jerkFact->setRawValue(_origJerk);
        }
    }

    _clearJournal(_vehicle->id());
    _applied = false;
    _accelName.clear();
    _jerkName.clear();

    qCInfo(VehicleParamOverrideLog) << "Spiral override restored";
    return true;
}

QString VehicleParamOverride::_journalPath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("maglab_param_override.json"));
}

void VehicleParamOverride::_writeJournal(int sysid)
{
    QFile file(_journalPath());
    QJsonObject root;
    if (file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    QJsonObject entry;
    entry[_accelName] = _origAccel;
    if (!_jerkName.isEmpty()) {
        entry[_jerkName] = _origJerk;
    }
    root[QString::number(sysid)] = entry;

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
    }
}

void VehicleParamOverride::_clearJournal(int sysid)
{
    QFile file(_journalPath());
    QJsonObject root;
    if (file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    root.remove(QString::number(sysid));

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
    }
}

void VehicleParamOverride::_recoverStaleJournal(Vehicle* vehicle)
{
    QFile file(_journalPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    const QJsonObject entry = root[QString::number(vehicle->id())].toObject();
    if (entry.isEmpty()) {
        return;
    }

    ParameterManager* pm = vehicle->parameterManager();
    const int compId = vehicle->defaultComponentId();
    for (const QString& paramName : entry.keys()) {
        const double value = entry[paramName].toDouble();
        if (!std::isfinite(value) || value <= 0.0) {
            continue;
        }
        Fact* fact = pm->getParameter(compId, paramName);
        if (fact != nullptr) {
            fact->setRawValue(value);
        }
    }

    _clearJournal(vehicle->id());

    qCWarning(VehicleParamOverrideLog) << "Recovered stranded WPNAV/WP override for vehicle" << vehicle->id();
}
