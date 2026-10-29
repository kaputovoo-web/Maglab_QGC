/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "AttitudeAnglesController.h"

#include "Fact.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QtMath>

QGC_LOGGING_CATEGORY(AttitudeAnglesControllerLog, "qgc.qmlcontrols.attitudeanglescontroller")

AttitudeAnglesController::AttitudeAnglesController(QObject *parent)
    : QObject(parent)
{
    MultiVehicleManager *const multiVehicleManager = MultiVehicleManager::instance();
    (void) connect(multiVehicleManager, &MultiVehicleManager::activeVehicleChanged, this, &AttitudeAnglesController::_setActiveVehicle);
    _setActiveVehicle(multiVehicleManager->activeVehicle());
}

AttitudeAnglesController::~AttitudeAnglesController()
{
    _disconnectFactSignals();
}

bool AttitudeAnglesController::activeVehicleAvailable() const
{
    return !_activeVehicle.isNull();
}

QString AttitudeAnglesController::pitchText() const
{
    return _formatAngle(_pitch, 1);
}

QString AttitudeAnglesController::rollText() const
{
    return _formatAngle(_roll, 1);
}

QString AttitudeAnglesController::yawText() const
{
    return _formatAngle(_yaw, 0);
}

void AttitudeAnglesController::_setActiveVehicle(Vehicle *vehicle)
{
    const bool oldAvailable = activeVehicleAvailable();

    _disconnectFactSignals();
    _activeVehicle = vehicle;

    if (_activeVehicle) {
        _pitchConnection = connect(_activeVehicle->pitch(), &Fact::rawValueChanged, this, &AttitudeAnglesController::_updateAttitude);
        _rollConnection = connect(_activeVehicle->roll(), &Fact::rawValueChanged, this, &AttitudeAnglesController::_updateAttitude);
        _yawConnection = connect(_activeVehicle->heading(), &Fact::rawValueChanged, this, &AttitudeAnglesController::_updateAttitude);
        _vehicleDestroyedConnection = connect(_activeVehicle, &QObject::destroyed, this, &AttitudeAnglesController::_clearActiveVehicle);
        _updateAttitude();
    } else {
        _pitch = qQNaN();
        _roll = qQNaN();
        _yaw = qQNaN();
        emit attitudeChanged();
    }

    if (oldAvailable != activeVehicleAvailable()) {
        emit activeVehicleAvailableChanged(activeVehicleAvailable());
    }
}

void AttitudeAnglesController::_updateAttitude()
{
    if (!_activeVehicle) {
        _clearActiveVehicle();
        return;
    }

    const double newPitch = _activeVehicle->pitch()->rawValue().toDouble();
    const double newRoll = _activeVehicle->roll()->rawValue().toDouble();
    const double newYaw = _activeVehicle->heading()->rawValue().toDouble();

    const bool changed =
        !qFuzzyCompare(1.0 + _pitch, 1.0 + newPitch) ||
        !qFuzzyCompare(1.0 + _roll, 1.0 + newRoll) ||
        !qFuzzyCompare(1.0 + _yaw, 1.0 + newYaw);

    _pitch = newPitch;
    _roll = newRoll;
    _yaw = newYaw;

    if (changed) {
        emit attitudeChanged();
    }
}

void AttitudeAnglesController::_clearActiveVehicle()
{
    _disconnectFactSignals();
    _activeVehicle = nullptr;
    _pitch = qQNaN();
    _roll = qQNaN();
    _yaw = qQNaN();
    emit attitudeChanged();
    emit activeVehicleAvailableChanged(false);
}

QString AttitudeAnglesController::_formatAngle(double value, int decimalPlaces) const
{
    if (!qIsFinite(value)) {
        return QStringLiteral("--");
    }

    return QStringLiteral("%1 deg").arg(QString::number(value, 'f', decimalPlaces));
}

void AttitudeAnglesController::_disconnectFactSignals()
{
    QObject::disconnect(_pitchConnection);
    QObject::disconnect(_rollConnection);
    QObject::disconnect(_yawConnection);
    QObject::disconnect(_vehicleDestroyedConnection);

    _pitchConnection = QMetaObject::Connection();
    _rollConnection = QMetaObject::Connection();
    _yawConnection = QMetaObject::Connection();
    _vehicleDestroyedConnection = QMetaObject::Connection();
}
