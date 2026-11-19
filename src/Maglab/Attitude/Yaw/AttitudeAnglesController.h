/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtQml/qqmlregistration.h>

class Vehicle;

Q_DECLARE_LOGGING_CATEGORY(AttitudeAnglesControllerLog)

class AttitudeAnglesController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool activeVehicleAvailable READ activeVehicleAvailable NOTIFY activeVehicleAvailableChanged)
    Q_PROPERTY(double pitch READ pitch NOTIFY attitudeChanged)
    Q_PROPERTY(double roll READ roll NOTIFY attitudeChanged)
    Q_PROPERTY(double yaw READ yaw NOTIFY attitudeChanged)
    Q_PROPERTY(QString pitchText READ pitchText NOTIFY attitudeChanged)
    Q_PROPERTY(QString rollText READ rollText NOTIFY attitudeChanged)
    Q_PROPERTY(QString yawText READ yawText NOTIFY attitudeChanged)

public:
    explicit AttitudeAnglesController(QObject *parent = nullptr);
    ~AttitudeAnglesController() override;

    bool activeVehicleAvailable() const;
    double pitch() const { return _pitch; }
    double roll() const { return _roll; }
    double yaw() const { return _yaw; }

    QString pitchText() const;
    QString rollText() const;
    QString yawText() const;

signals:
    void activeVehicleAvailableChanged(bool activeVehicleAvailable);
    void attitudeChanged();

private slots:
    void _setActiveVehicle(Vehicle *vehicle);
    void _updateAttitude();
    void _clearActiveVehicle();

private:
    QString _formatAngle(double value, int decimalPlaces) const;
    void _disconnectFactSignals();

    QPointer<Vehicle> _activeVehicle;
    QMetaObject::Connection _pitchConnection;
    QMetaObject::Connection _rollConnection;
    QMetaObject::Connection _yawConnection;
    QMetaObject::Connection _vehicleDestroyedConnection;

    double _pitch = qQNaN();
    double _roll = qQNaN();
    double _yaw = qQNaN();
};
