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

/// @brief Archimedean spiral trajectory strategy.
///
/// Approximates the spiral with a chain of tangent semicircles. Curvature is
/// constant inside each pi interval and the radius grows by spacing/2 at the
/// following interval.
class SpiralPlanStrategy : public LLMPlanStrategy
{
    Q_OBJECT

public:
    explicit SpiralPlanStrategy(QObject* parent = nullptr);

    QList<Fact*> facts() override;
    void recomputeGeometry(const QGeoCoordinate& center) override;
    QList<QGeoCoordinate> path() const override { return _displayPath; }
    QList<QGeoCoordinate> arcBoundaryPoints() const { return _arcBoundaryPoints; }
    QString editorQmlName() const override { return QStringLiteral("LLMSpiralEditor.qml"); }
    QJsonObject saveParams() const override;
    bool loadParams(const QJsonObject& params) override;
    double complexDistance() const override { return _complexDistance; }
    int lastSequenceNumber() const override { return _lastItemCount; }
    float neededAccelCmss() const { return _neededAccelCmss; }
    float plannedAccelCmss() const { return _neededAccelCmss; }
    float plannedJerkMsss() const { return _plannedJerkMsss; }
    double firstArcRadius() const;
    double safeEntrySpeed() const;
    double requestedEntryBankAngle() const;
    bool entrySpeedLimited() const;
    double estimatedFlightTime() const;
    double minAMSLAltitude() const override;
    double maxAMSLAltitude() const override;
    QList<MissionItem*> missionItems(double relAlt, MAV_FRAME frame, QObject* parent) override;
    Fact* flightSpeed() override { return &_flightSpeed; }
    double specifiedFlightSpeed() const override;
    Fact* altitudeMode() override { return &_altitudeMode; }

    Fact* startRadius() { return &_startRadius; }
    Fact* spacing() { return &_spacing; }
    Fact* turns() { return &_turns; }
    Fact* altitude() override { return &_altitude; }
    Fact* direction() { return &_direction; }
    Fact* maxBankAngle() { return &_maxBankAngle; }
    Fact* rotationAngle() { return &_rotationAngle; }

private slots:
    void _recomputeIfReady();

private:
    void _initMetaData();
    double _safeSpeedForRadius(double radius, double requestedSpeed) const;
    void _rebuildArcSpeedTargets();
    void _recomputeMissionItemCount();

    Fact _startRadius;
    Fact _spacing;
    Fact _turns;
    Fact _altitude;
    Fact _direction;
    Fact _flightSpeed;
    Fact _altitudeMode;
    Fact _maxBankAngle;
    Fact _rotationAngle;

    FactMetaData* _startRadiusMetaData = nullptr;
    FactMetaData* _spacingMetaData = nullptr;
    FactMetaData* _turnsMetaData = nullptr;
    FactMetaData* _altitudeMetaData = nullptr;
    FactMetaData* _directionMetaData = nullptr;
    FactMetaData* _flightSpeedMetaData = nullptr;
    FactMetaData* _altitudeModeMetaData = nullptr;
    FactMetaData* _maxBankAngleMetaData = nullptr;
    FactMetaData* _rotationAngleMetaData = nullptr;

    QList<QGeoCoordinate> _displayPath;
    QList<QGeoCoordinate> _arcBoundaryPoints;
    QList<double> _pointRadius;
    QList<int> _pointArc;
    QVector<double> _arcTargetSpeed;
    int _numArcs = 0;
    double _spiralB = 0.0;
    double _firstArcRadius = 3.0;
    double _complexDistance = 0.0;
    float _neededAccelCmss = 100.0f;
    float _plannedJerkMsss = 5.0f;
    int _lastItemCount = 0;
    QGeoCoordinate _lastCenter;

    // tan(phi)=v^2/(g*r). The planner reserves 20% lateral-acceleration
    // headroom for wind, tracking error and spline interpolation.
    static constexpr double kGravity = 9.81;
    static constexpr double kBankSafetyFactor = 0.80;
    // Hard cap on the lateral-acceleration budget (m/s²) used to derive the
    // spiral cruise speed. Matches the WPNAV_ACCEL override (400 cm/s²) applied
    // by VehicleParamOverride with headroom, so the commanded speed stays
    // deliverable regardless of the Max Bank Angle setting.
    static constexpr double kMaxLatAccel = 3.5;
    static constexpr double kVMin = 0.8;
    static constexpr double kSpeedEmitDelta = 0.4;
    static constexpr int kMaxPoints = 520;
    static constexpr double kMaxAngularStepDeg = 25.0;
};
