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

#include <QVector>

/// @brief FOM (Figure-of-Merit) attitude-disturbance trajectory.
///
/// Flies a square; each edge applies a configurable disturbance about one axis so
/// the airframe sees roll / pitch / yaw excitation in turn (sensor/IMU checks):
///   - roll  edge: lateral weave (path curvature → bank, a = g·tanφ),
///   - pitch edge: fore/aft speed oscillation (accel/decel → pitch),
///   - yaw   edge: real MAV_CMD_CONDITION_YAW heading wiggle.
/// Each axis has an amplitude (slider) and an independent safety cap. roll/pitch are
/// motion approximations (uploadable AUTO mission); only yaw is a direct command.
class FomPlanStrategy : public LLMPlanStrategy
{
    Q_OBJECT

public:
    explicit FomPlanStrategy(QObject* parent = nullptr);

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

    Fact* sideLength()  { return &_sideLength; }
    Fact* altitude() override { return &_altitude; }

private slots:
    void _recomputeIfReady();

private:
    void _initMetaData();
    static FactMetaData* _angleMeta(QObject* p, const QString& desc, double def);

    Fact _sideLength;
    Fact _flightSpeed;
    Fact _altitude;
    Fact _altitudeMode;
    Fact _cycles;
    Fact _rollAmp;   Fact _rollLimit;
    Fact _pitchAmp;  Fact _pitchLimit;
    Fact _yawAmp;    Fact _yawLimit;

    FactMetaData* _sideLengthMetaData   = nullptr;
    FactMetaData* _flightSpeedMetaData   = nullptr;
    FactMetaData* _altitudeMetaData      = nullptr;
    FactMetaData* _altitudeModeMetaData  = nullptr;
    FactMetaData* _cyclesMetaData        = nullptr;
    FactMetaData* _rollAmpMeta = nullptr;  FactMetaData* _rollLimitMeta = nullptr;
    FactMetaData* _pitchAmpMeta = nullptr; FactMetaData* _pitchLimitMeta = nullptr;
    FactMetaData* _yawAmpMeta = nullptr;   FactMetaData* _yawLimitMeta = nullptr;

    // Square corners (geo) in flight order, and a sampled display path (with the
    // roll-edge weave drawn so the map shows it).
    QList<QGeoCoordinate> _corners;
    QList<QGeoCoordinate> _path;
    double _complexDistance = 0.0;
    int    _lastItemCount   = 0;
    QGeoCoordinate _lastCenter;

    static constexpr double kGravity = 9.81;
};
