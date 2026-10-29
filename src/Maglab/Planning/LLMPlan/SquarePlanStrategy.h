/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "LLMPlanStrategy.h"
#include "FactMetaData.h"

/// @brief Expanding-square (box-spiral) search trajectory — classic SAR pattern.
///
/// Starts at the centre and flies outward in a square spiral: leg lengths grow
/// by `spacing` every two legs, turning 90° each time. Corners are flown as
/// spline waypoints so the autopilot rounds them for a smooth path.
class SquarePlanStrategy : public LLMPlanStrategy
{
    Q_OBJECT

public:
    explicit SquarePlanStrategy(QObject* parent = nullptr);

    QList<Fact*> facts() override;
    void recomputeGeometry(const QGeoCoordinate& center) override;
    QList<QGeoCoordinate> path() const override { return _path; }
    QString editorQmlName() const override { return QString(); }
    QJsonObject saveParams() const override;
    bool loadParams(const QJsonObject& params) override;
    double complexDistance() const override { return _complexDistance; }
    int lastSequenceNumber() const override { return _lastItemCount; }
    double minAMSLAltitude() const override;
    double maxAMSLAltitude() const override;
    QList<MissionItem*> missionItems(double relAlt, MAV_FRAME frame, QObject* parent) override;
    Fact* flightSpeed() override { return &_flightSpeed; }
    double specifiedFlightSpeed() const override;
    Fact* altitudeMode() override { return &_altitudeMode; }

    // Fact accessors for the shell's strategyFact() lookup
    Fact* spacing()       { return &_spacing; }
    Fact* legs()          { return &_legs; }
    Fact* altitude() override { return &_altitude; }
    Fact* direction()     { return &_direction; }
    Fact* rotationAngle() { return &_rotationAngle; }

private slots:
    void _recomputeIfReady();

private:
    void _initMetaData();

    Fact _spacing;
    Fact _legs;
    Fact _altitude;
    Fact _direction;
    Fact _flightSpeed;
    Fact _altitudeMode;
    Fact _rotationAngle;

    FactMetaData* _spacingMetaData      = nullptr;
    FactMetaData* _legsMetaData         = nullptr;
    FactMetaData* _altitudeMetaData     = nullptr;
    FactMetaData* _directionMetaData    = nullptr;
    FactMetaData* _flightSpeedMetaData  = nullptr;
    FactMetaData* _altitudeModeMetaData = nullptr;
    FactMetaData* _rotationAngleMetaData = nullptr;

    QList<QGeoCoordinate> _path;
    double _complexDistance = 0.0;
    int    _lastItemCount   = 0;
    QGeoCoordinate _lastCenter;
};
