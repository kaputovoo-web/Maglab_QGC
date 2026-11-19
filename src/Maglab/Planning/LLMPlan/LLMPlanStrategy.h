/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QList>
#include <QGeoCoordinate>
#include <QJsonObject>
#include <QVariantList>

#include "Fact.h"
#include "MissionItem.h"

/// @brief Abstract base class for LLM sub-mode strategies.
///
/// Each sub-mode (Voice, Spiral, FOM) is a concrete subclass that provides
/// its own set of parameter Facts, geometry computation, and JSON serialization.
///
/// To add a new sub-mode:
///   1. Subclass LLMPlanStrategy and implement all pure virtuals.
///   2. Add an enum value to LLMComplexItem::PlanMode.
///   3. Add factory entry in LLMComplexItem::_initStrategies().
///   4. Add a QML editor file and register in qgroundcontrol.qrc.
class LLMPlanStrategy : public QObject
{
    Q_OBJECT

public:
    LLMPlanStrategy(QObject* parent = nullptr)
        : QObject(parent) {}
    ~LLMPlanStrategy() override = default;

    /// @return All Facts exposed to the editor QML for this strategy.
    virtual QList<Fact*> facts() = 0;

    /// Recompute the geometry points from current Fact values.
    /// @param center The mission center coordinate.
    virtual void recomputeGeometry(const QGeoCoordinate& center) = 0;

    /// @return The computed flight path as a list of WGS84 coordinates.
    virtual QList<QGeoCoordinate> path() const = 0;

    /// @return The QML file name for this strategy's sub-editor panel
    /// (e.g. "LLMSpiralEditor.qml"). Resolved via QRC alias.
    virtual QString editorQmlName() const = 0;

    /// Serialize this strategy's parameter values.
    virtual QJsonObject saveParams() const = 0;

    /// Restore this strategy's parameter values from JSON.
    /// @return true on success, false on parse error.
    virtual bool loadParams(const QJsonObject& params) = 0;

    /// @return Total flight distance in meters.
    virtual double complexDistance() const = 0;

    /// @return Exact number of MAVLink mission items emitted by missionItems().
    virtual int lastSequenceNumber() const = 0;

    /// @return Minimum AMSL altitude of all points.
    virtual double minAMSLAltitude() const = 0;

    /// @return Maximum AMSL altitude of all points.
    virtual double maxAMSLAltitude() const = 0;

    /// Build MAVLink waypoints for this strategy.
    /// @param relAlt Relative altitude (m above home) for NAV_WAYPOINT param7.
    /// @param frame The MAV_FRAME to use (depends on altitudeMode).
    /// @param parent QObject parent for created MissionItems.
    /// @return List of newly-allocated MissionItem pointers (caller owns).
    virtual QList<MissionItem*> missionItems(double relAlt, MAV_FRAME frame, QObject* parent) = 0;

    /// @return The flight speed Fact for this strategy (m/s, groundspeed).
    virtual Fact* flightSpeed() = 0;

    /// @return The flight speed value in m/s, or NaN if not specified.
    virtual double specifiedFlightSpeed() const = 0;

    /// @return The altitude mode Fact (AltitudeModeRelative or AltitudeModeAbsolute).
    virtual Fact* altitudeMode() = 0;
    /// @return Editable cruise altitude Fact, or nullptr when the mode has none.
    virtual Fact* altitude() = 0;

signals:
    /// Emitted whenever the geometry (path) changes due to param or center update.
    void geometryChanged();

    /// Emitted whenever complexDistance changes.
    void complexDistanceChanged();

    /// Emitted whenever lastSequenceNumber changes.
    void lastSequenceNumberChanged();
};
