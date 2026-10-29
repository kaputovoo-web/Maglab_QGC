#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include "QGCLoggingCategory.h"

class Vehicle;
class MultiVehicleManager;

Q_DECLARE_LOGGING_CATEGORY(VehicleParamOverrideLog)

/// @brief Temporarily raises WPNAV_ACCEL / WPNAV_JERK while a spiral mission is
/// flying, and restores the originals when the vehicle leaves Auto or disarms.
///
/// Spiral missions (tight 5-6 m radius arcs) demand more lateral acceleration
/// than ArduPilot's default WPNAV_ACCEL (250 cm/s²) and WPNAV_JERK (1 m/s³)
/// allow, which makes the vehicle jitter at every waypoint. This class applies
/// a conservative override only while the active vehicle is armed in Auto AND
/// its loaded mission contains spline (spiral) waypoints, then restores the
/// original values. Originals are journaled to disk so a crash/relink cannot
/// leave the vehicle with modified parameters.
class VehicleParamOverride : public QObject
{
    Q_OBJECT

public:
    static VehicleParamOverride* instance();

    /// Connect to the active-vehicle lifecycle. Called once at app boot.
    void init(MultiVehicleManager* multiVehicleManager);

signals:
    /// Emitted with a human-readable message when an override or restore fails
    /// (e.g. a parameter write was rejected). The flight is never aborted by
    /// this class — callers surface this as a strong warning instead.
    void warning(const QString& message);

private:
    explicit VehicleParamOverride(QObject* parent = nullptr);

    void _setActiveVehicle(Vehicle* vehicle);
    void _onFlightModeChanged(const QString& mode);
    void _onArmedChanged(bool armed);
    void _onParametersReady(bool ready);

    bool _missionContainsSpiral() const;
    bool _apply();
    bool _restore();

    QString _journalPath() const;
    void _writeJournal(int sysid);
    void _clearJournal(int sysid);
    void _recoverStaleJournal(Vehicle* vehicle);

    QPointer<Vehicle> _vehicle;
    bool _initialized = false;
    bool _applied = false;
    double _origAccel = 0.0;
    double _origJerk = 0.0;
    QString _accelName;   ///< Resolved parameter name (WPNAV_ACCEL or WP_ACCEL)
    QString _jerkName;    ///< Resolved parameter name (WPNAV_JERK or WP_JERK), empty when unavailable

    static VehicleParamOverride* s_instance;
};
