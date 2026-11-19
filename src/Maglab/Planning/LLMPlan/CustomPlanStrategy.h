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

#include <QPointF>

/// @brief Custom trajectory generated from LLM-supplied relative offsets.
///
/// The LLM outputs a list of east/north offsets (meters) from a center point.
/// This strategy converts them to WGS84 via atDistanceAndAzimuth() and creates
/// MAV_CMD_NAV_WAYPOINT items.
class CustomPlanStrategy : public LLMPlanStrategy
{
    Q_OBJECT
public:
    explicit CustomPlanStrategy(QObject* parent = nullptr);

    QList<Fact*> facts() override { return {&_altitude, &_altitudeMode, &_flightSpeed}; }
    void recomputeGeometry(const QGeoCoordinate& center) override;
    QList<QGeoCoordinate> path() const override { return _path; }
    QString editorQmlName() const override { return QString(); }
    QJsonObject saveParams() const override;
    bool loadParams(const QJsonObject& params) override;
    double complexDistance() const override { return _complexDistance; }
    int lastSequenceNumber() const override { return _path.isEmpty() ? 0 : _path.size() + 1; }
    double minAMSLAltitude() const override { return _altitude.rawValue().toDouble(); }
    double maxAMSLAltitude() const override { return _altitude.rawValue().toDouble(); }
    QList<MissionItem*> missionItems(double relAlt, MAV_FRAME frame, QObject* parent) override;
    Fact* flightSpeed() override { return &_flightSpeed; }
    double specifiedFlightSpeed() const override;
    Fact* altitudeMode() override { return &_altitudeMode; }

    void setOffsets(const QList<QPointF>& offsets);
    void setCloseLoop(bool close) { _closeLoop = close; }
    QList<QPointF> offsets() const { return _offsets; }

    Fact* altitude() override { return &_altitude; }

private:
    void _initMetaData();

    Fact _altitude;
    Fact _flightSpeed;
    Fact _altitudeMode;
    FactMetaData* _altitudeMetaData     = nullptr;
    FactMetaData* _flightSpeedMetaData  = nullptr;
    FactMetaData* _altitudeModeMetaData = nullptr;

    QList<QPointF>       _offsets;
    QList<QGeoCoordinate> _path;
    double _complexDistance = 0.0;
    bool   _closeLoop       = false;
    QGeoCoordinate _lastCenter;
};
