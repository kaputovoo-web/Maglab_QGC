/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "SpiralPlanStrategy.h"
#include "PlanParameterValidation.h"

#include <QtMath>
#include <QGeoCoordinate>
#include <limits>

SpiralPlanStrategy::SpiralPlanStrategy(QObject* parent)
    : LLMPlanStrategy(parent)
    , _startRadius(0, QStringLiteral("startRadius"), FactMetaData::valueTypeDouble, this)
    , _spacing(0, QStringLiteral("spacing"), FactMetaData::valueTypeDouble, this)
    , _turns(0, QStringLiteral("turns"), FactMetaData::valueTypeDouble, this)
    , _altitude(0, QStringLiteral("altitude"), FactMetaData::valueTypeDouble, this)
    , _direction(0, QStringLiteral("direction"), FactMetaData::valueTypeInt32, this)
    , _flightSpeed(0, QStringLiteral("flightSpeed"), FactMetaData::valueTypeDouble, this)
    , _altitudeMode(0, QStringLiteral("altitudeMode"), FactMetaData::valueTypeInt32, this)
    , _maxBankAngle(0, QStringLiteral("maxBankAngle"), FactMetaData::valueTypeDouble, this)
    , _rotationAngle(0, QStringLiteral("rotationAngle"), FactMetaData::valueTypeDouble, this)
{
    _initMetaData();

    connect(&_startRadius, &Fact::valueChanged, this, &SpiralPlanStrategy::_recomputeIfReady);
    connect(&_spacing, &Fact::valueChanged, this, &SpiralPlanStrategy::_recomputeIfReady);
    connect(&_turns, &Fact::valueChanged, this, &SpiralPlanStrategy::_recomputeIfReady);
    connect(&_altitude, &Fact::valueChanged, this, &SpiralPlanStrategy::_recomputeIfReady);
    connect(&_direction, &Fact::valueChanged, this, &SpiralPlanStrategy::_recomputeIfReady);
    connect(&_rotationAngle, &Fact::valueChanged, this, &SpiralPlanStrategy::_recomputeIfReady);
    connect(&_flightSpeed, &Fact::valueChanged, this, &SpiralPlanStrategy::_recomputeIfReady);
    connect(&_maxBankAngle, &Fact::valueChanged, this, &SpiralPlanStrategy::_recomputeIfReady);
}

void SpiralPlanStrategy::_initMetaData()
{
    _startRadiusMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _startRadiusMetaData->setShortDescription(tr("First Arc Radius (0 = spacing)"));
    _startRadiusMetaData->setRawMin(0.0);
    _startRadiusMetaData->setRawMax(1000.0);
    _startRadiusMetaData->setRawDefaultValue(0.0);
    _startRadiusMetaData->setDecimalPlaces(1);
    _startRadiusMetaData->setRawUnits("m");
    _startRadius.setMetaData(_startRadiusMetaData, true);

    _spacingMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _spacingMetaData->setShortDescription(tr("Spacing"));
    _spacingMetaData->setRawMin(1.0);
    _spacingMetaData->setRawMax(500.0);
    _spacingMetaData->setRawDefaultValue(25.0);
    _spacingMetaData->setDecimalPlaces(1);
    _spacingMetaData->setRawUnits("m");
    _spacing.setMetaData(_spacingMetaData, true);

    _turnsMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _turnsMetaData->setShortDescription(tr("Turns"));
    _turnsMetaData->setRawMin(0.5);
    _turnsMetaData->setRawMax(50.0);
    _turnsMetaData->setRawDefaultValue(5.0);
    _turnsMetaData->setDecimalPlaces(1);
    _turns.setMetaData(_turnsMetaData, true);

    _altitudeMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _altitudeMetaData->setShortDescription(tr("Altitude"));
    _altitudeMetaData->setRawMin(0.0);
    _altitudeMetaData->setRawMax(400.0);
    _altitudeMetaData->setRawDefaultValue(80.0);
    _altitudeMetaData->setDecimalPlaces(1);
    _altitudeMetaData->setRawUnits("m");
    _altitude.setMetaData(_altitudeMetaData, true);

    _directionMetaData = new FactMetaData(FactMetaData::valueTypeInt32, this);
    _directionMetaData->setShortDescription(tr("Direction"));
    _directionMetaData->setRawDefaultValue(0);
    _directionMetaData->setEnumInfo(QStringList{tr("Clockwise"), tr("Counter-Clockwise")}, {0, 1});
    _direction.setMetaData(_directionMetaData, true);

    _flightSpeedMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _flightSpeedMetaData->setShortDescription(tr("Requested Flight Speed"));
    _flightSpeedMetaData->setRawMin(1.0);
    _flightSpeedMetaData->setRawMax(30.0);
    _flightSpeedMetaData->setRawDefaultValue(8.0);
    _flightSpeedMetaData->setDecimalPlaces(1);
    _flightSpeedMetaData->setRawUnits("m/s");
    _flightSpeed.setMetaData(_flightSpeedMetaData, true);

    _altitudeModeMetaData = new FactMetaData(FactMetaData::valueTypeInt32, this);
    _altitudeModeMetaData->setShortDescription(tr("Altitude Mode"));
    _altitudeModeMetaData->setRawDefaultValue(0);
    _altitudeModeMetaData->setEnumInfo(QStringList{tr("Relative"), tr("AMSL"), tr("Terrain")}, {0, 1, 2});
    _altitudeMode.setMetaData(_altitudeModeMetaData, true);

    _maxBankAngleMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _maxBankAngleMetaData->setShortDescription(tr("Maximum Bank Angle"));
    _maxBankAngleMetaData->setRawMin(5.0);
    _maxBankAngleMetaData->setRawMax(35.0);
    _maxBankAngleMetaData->setRawDefaultValue(25.0);
    _maxBankAngleMetaData->setDecimalPlaces(1);
    _maxBankAngleMetaData->setRawUnits(QStringLiteral("\302\260"));
    _maxBankAngle.setMetaData(_maxBankAngleMetaData, true);

    _rotationAngleMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _rotationAngleMetaData->setShortDescription(tr("Rotation"));
    _rotationAngleMetaData->setRawMin(0.0);
    _rotationAngleMetaData->setRawMax(360.0);
    _rotationAngleMetaData->setRawDefaultValue(0.0);
    _rotationAngleMetaData->setDecimalPlaces(0);
    _rotationAngleMetaData->setRawUnits(QStringLiteral("\302\260"));
    _rotationAngle.setMetaData(_rotationAngleMetaData, true);
}

QList<Fact*> SpiralPlanStrategy::facts()
{
    return {&_startRadius, &_spacing, &_turns, &_altitude, &_altitudeMode,
            &_direction, &_rotationAngle, &_flightSpeed, &_maxBankAngle};
}

double SpiralPlanStrategy::specifiedFlightSpeed() const
{
    return _flightSpeed.rawValue().toDouble();
}

double SpiralPlanStrategy::firstArcRadius() const
{
    return _firstArcRadius;
}

double SpiralPlanStrategy::safeEntrySpeed() const
{
    return _arcTargetSpeed.isEmpty() ? 0.0 : _arcTargetSpeed.first();
}

double SpiralPlanStrategy::requestedEntryBankAngle() const
{
    const double speed = qMax(0.0, safeEntrySpeed());
    if (_firstArcRadius <= 1e-6) {
        return 90.0;
    }
    return qRadiansToDegrees(qAtan((speed * speed) / (kGravity * _firstArcRadius)));
}

bool SpiralPlanStrategy::entrySpeedLimited() const
{
    return safeEntrySpeed() + 0.05 < specifiedFlightSpeed();
}

double SpiralPlanStrategy::estimatedFlightTime() const
{
    if (_displayPath.isEmpty() || _arcTargetSpeed.isEmpty()) {
        return 0.0;
    }

    // The brief entry hold is part of the mission timing; every segment flies
    // at the single constant cruise speed.
    double seconds = 0.5;
    for (int i = 1; i < _displayPath.size(); ++i) {
        const int arc = qBound(0, _pointArc.value(i, 0), _arcTargetSpeed.size() - 1);
        seconds += _displayPath[i - 1].distanceTo(_displayPath[i])
                 / qMax(0.3, _arcTargetSpeed[arc]);
    }
    return seconds;
}

void SpiralPlanStrategy::recomputeGeometry(const QGeoCoordinate& center)
{
    _lastCenter = center;
    _recomputeIfReady();
}

void SpiralPlanStrategy::_recomputeIfReady()
{
    if (!_lastCenter.isValid()) {
        return;
    }

    const double spacing = _spacing.rawValue().toDouble();
    const double turns = _turns.rawValue().toDouble();
    const int direction = _direction.rawValue().toInt();
    const double rotation = _rotationAngle.rawValue().toDouble();
    const double configuredFirstRadius = _startRadius.rawValue().toDouble();

    if (spacing <= 0.0 || turns <= 0.0) {
        _displayPath.clear();
        _arcBoundaryPoints.clear();
        _pointRadius.clear();
        _pointArc.clear();
        _arcTargetSpeed.clear();
        _numArcs = 0;
        _lastItemCount = 0;
        _complexDistance = 0.0;
        emit geometryChanged();
        emit complexDistanceChanged();
        emit lastSequenceNumberChanged();
        return;
    }

    // The attachment defines r_k = r_0 + k*s/2. A zero r_0 means "use
    // spacing", which gives the documented r_0=s default without hiding a
    // geometry change from the operator.
    _firstArcRadius = qMax(1.0, configuredFirstRadius > 0.0 ? configuredFirstRadius : spacing);
    const double totalTheta = turns * 2.0 * M_PI;
    const int halfTurnCount = qMax(1, static_cast<int>(qCeil(totalTheta / M_PI)));
    _spiralB = spacing / (2.0 * M_PI);

    double sampleDistance = qBound(6.0, spacing * 0.25, 12.0);
    double estimatedLength = 0.0;
    for (int arc = 0; arc < halfTurnCount; ++arc) {
        const double arcStart = arc * M_PI;
        const double arcSweep = qMin(M_PI, totalTheta - arcStart);
        const double arcRadius = _firstArcRadius + arc * spacing * 0.5;
        estimatedLength += arcRadius * qMax(arcSweep, 0.0);
    }
    if (sampleDistance > 0.0 && estimatedLength / sampleDistance > kMaxPoints) {
        sampleDistance = estimatedLength / static_cast<double>(kMaxPoints);
    }

    _displayPath.clear();
    _arcBoundaryPoints.clear();
    _pointRadius.clear();
    _pointArc.clear();
    _complexDistance = 0.0;

    const double directionSign = (direction == 0) ? 1.0 : -1.0;
    const double maxAngularStep = qDegreesToRadians(kMaxAngularStepDeg);
    QGeoCoordinate previousPoint;
    bool firstPoint = true;

    for (int arc = 0; arc < halfTurnCount; ++arc) {
        const double arcStart = arc * M_PI;
        const double arcSweep = qMin(M_PI, totalTheta - arcStart);
        if (arcSweep <= 1e-9) {
            break;
        }

        // Adjacent circle centres alternate by s/4 about the requested centre.
        // Their separation is s/2, equal to r_(k+1)-r_k, so position and
        // tangent are continuous at every pi boundary.
        const double centerAzimuth = rotation + (arc + 1) * 180.0;
        const QGeoCoordinate arcCenter =
                _lastCenter.atDistanceAndAzimuth(spacing * 0.25, centerAzimuth);
        const double arcRadius = _firstArcRadius + arc * spacing * 0.5;
        const double boundaryAzimuth =
                rotation + directionSign * qRadiansToDegrees(arcStart);
        _arcBoundaryPoints.append(
                arcCenter.atDistanceAndAzimuth(arcRadius, boundaryAzimuth));

        const double angularStep =
                qMin(sampleDistance / qMax(arcRadius, 0.1), maxAngularStep);
        const int samples = qMax(1, static_cast<int>(qCeil(arcSweep / angularStep)));

        for (int sample = (arc == 0 ? 0 : 1); sample <= samples; ++sample) {
            const double localTheta = arcSweep * sample / samples;
            const double pointAzimuth =
                    rotation + directionSign * qRadiansToDegrees(arcStart + localTheta);
            const QGeoCoordinate point =
                    arcCenter.atDistanceAndAzimuth(arcRadius, pointAzimuth);
            _displayPath.append(point);
            _pointRadius.append(arcRadius);
            _pointArc.append(arc);
            if (!firstPoint) {
                _complexDistance += previousPoint.distanceTo(point);
            }
            previousPoint = point;
            firstPoint = false;
        }
    }

    if (!_displayPath.isEmpty()) {
        _arcBoundaryPoints.append(_displayPath.last());
    }
    _numArcs = halfTurnCount;
    _rebuildArcSpeedTargets();
    _recomputeMissionItemCount();

    emit geometryChanged();
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged();
}

double SpiralPlanStrategy::_safeSpeedForRadius(double radius, double requestedSpeed) const
{
    const double bankDeg = qBound(5.0, _maxBankAngle.rawValue().toDouble(), 35.0);
    const double lateralBudget =
            qMin(kGravity * qTan(qDegreesToRadians(bankDeg)) * kBankSafetyFactor,
                 kMaxLatAccel);
    const double curveLimit = qSqrt(qMax(0.0, lateralBudget * qMax(radius, 0.0)));
    return qMax(0.3, qMin(requestedSpeed, curveLimit));
}

void SpiralPlanStrategy::_rebuildArcSpeedTargets()
{
    _arcTargetSpeed.clear();
    const double requestedSpeed = qMax(0.3, _flightSpeed.rawValue().toDouble());
    const double bankDeg = qBound(5.0, _maxBankAngle.rawValue().toDouble(), 35.0);
    const double lateralBudget =
            qMin(kGravity * qTan(qDegreesToRadians(bankDeg)) * kBankSafetyFactor,
                 kMaxLatAccel);

    _neededAccelCmss = static_cast<float>(lateralBudget * 100.0);
    _plannedJerkMsss = static_cast<float>(qBound(1.0, lateralBudget * 1.25, 6.0));

    // Constant-speed spiral: fly every arc at the speed the tightest (inner)
    // radius safely allows. Uniform ground speed is better for magnetic
    // sampling and removes the per-arc DO_CHANGE_SPEED "pumping" that jitters
    // the vehicle at each waypoint on tight spirals.
    const double constantSpeed = _safeSpeedForRadius(_firstArcRadius, requestedSpeed);
    _arcTargetSpeed.reserve(_numArcs);
    for (int arc = 0; arc < _numArcs; ++arc) {
        _arcTargetSpeed.append(constantSpeed);
    }
}

void SpiralPlanStrategy::_recomputeMissionItemCount()
{
    if (_displayPath.isEmpty() || _pointArc.isEmpty() || _arcTargetSpeed.isEmpty()) {
        _lastItemCount = 0;
        return;
    }

    // Entry speed + first waypoint hold.
    int count = 2;
    double commandedSpeed = _arcTargetSpeed.first();
    int previousArc = _pointArc.first();
    const double requestedSpeed = specifiedFlightSpeed();

    for (int i = 1; i < _displayPath.size(); ++i) {
        const int arc = _pointArc.value(i, previousArc);
        if (arc != previousArc && arc >= 0 && arc < _arcTargetSpeed.size()) {
            const double nextSpeed = _arcTargetSpeed[arc];
            const bool reachedRequested =
                    qAbs(nextSpeed - requestedSpeed) < 0.05 &&
                    qAbs(commandedSpeed - requestedSpeed) >= 0.05;
            if (qAbs(nextSpeed - commandedSpeed) >= kSpeedEmitDelta || reachedRequested) {
                ++count;
                commandedSpeed = nextSpeed;
            }
            previousArc = arc;
        }
        ++count; // spline waypoint
    }

    _lastItemCount = count;
}

double SpiralPlanStrategy::minAMSLAltitude() const
{
    return _altitude.rawValue().toDouble();
}

double SpiralPlanStrategy::maxAMSLAltitude() const
{
    return _altitude.rawValue().toDouble();
}

QList<MissionItem*> SpiralPlanStrategy::missionItems(double relAlt, MAV_FRAME frame, QObject* parent)
{
    QList<MissionItem*> items;
    if (_displayPath.isEmpty()) {
        if (_lastCenter.isValid()) {
            recomputeGeometry(_lastCenter);
        }
        if (_displayPath.isEmpty() || _arcTargetSpeed.isEmpty()) {
            return items;
        }
    }

    const double spacing = _spacing.rawValue().toDouble();
    const int pointCount = _displayPath.size();
    const double entryAcceptance = qMin(qBound(1.2, spacing * 0.25, 8.0), 2.0);
    const double cruiseSpeed = _arcTargetSpeed.first();

    // Fly to the first semicircle start at the single cruise speed, hold
    // briefly, then follow the spline. One DO_CHANGE_SPEED for the whole spiral
    // keeps ground speed uniform and avoids per-arc speed stepping.
    items.append(new MissionItem(0, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
                                  1, cruiseSpeed, -1, 0, 0, 0, 0,
                                  true, false, parent));
    items.append(new MissionItem(0, MAV_CMD_NAV_WAYPOINT, frame,
                                  0.5, entryAcceptance, 0.0, 0.0,
                                  _displayPath[0].latitude(), _displayPath[0].longitude(), relAlt,
                                  true, false, parent));

    double commandedSpeed = _arcTargetSpeed.first();
    int previousArc = _pointArc.value(0, 0);
    const double requestedSpeed = specifiedFlightSpeed();

    for (int i = 1; i < pointCount; ++i) {
        const int arc = _pointArc.value(i, previousArc);
        if (arc != previousArc && arc >= 0 && arc < _arcTargetSpeed.size()) {
            const double nextSpeed = _arcTargetSpeed[arc];
            const bool reachedRequested =
                    qAbs(nextSpeed - requestedSpeed) < 0.05 &&
                    qAbs(commandedSpeed - requestedSpeed) >= 0.05;
            if (qAbs(nextSpeed - commandedSpeed) >= kSpeedEmitDelta || reachedRequested) {
                items.append(new MissionItem(0, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
                                             1, nextSpeed, -1, 0, 0, 0, 0,
                                             true, false, parent));
                commandedSpeed = nextSpeed;
            }
            previousArc = arc;
        }

        items.append(new MissionItem(0, MAV_CMD_NAV_SPLINE_WAYPOINT, frame,
                                      0.0, 0.0, 0.0, 0.0,
                                      _displayPath[i].latitude(), _displayPath[i].longitude(), relAlt,
                                      true, false, parent));
    }


    _lastItemCount = items.size();
    return items;
}

QJsonObject SpiralPlanStrategy::saveParams() const
{
    QJsonObject object;
    object[QStringLiteral("startRadius")] = _startRadius.rawValue().toDouble();
    object[QStringLiteral("spacing")] = _spacing.rawValue().toDouble();
    object[QStringLiteral("turns")] = _turns.rawValue().toDouble();
    object[QStringLiteral("altitude")] = _altitude.rawValue().toDouble();
    object[QStringLiteral("direction")] = _direction.rawValue().toInt();
    object[QStringLiteral("flightSpeed")] = _flightSpeed.rawValue().toDouble();
    object[QStringLiteral("altitudeMode")] = _altitudeMode.rawValue().toInt();
    object[QStringLiteral("maxBankAngle")] = _maxBankAngle.rawValue().toDouble();
    object[QStringLiteral("rotationAngle")] = _rotationAngle.rawValue().toDouble();
    return object;
}

bool SpiralPlanStrategy::loadParams(const QJsonObject& params)
{
    if (!PlanParameterValidation::spiral(params)) {
        return false;
    }
    if (params.contains(QStringLiteral("startRadius"))) {
        _startRadius.setRawValue(params[QStringLiteral("startRadius")].toDouble());
    }
    if (params.contains(QStringLiteral("spacing"))) {
        _spacing.setRawValue(params[QStringLiteral("spacing")].toDouble());
    }
    if (params.contains(QStringLiteral("turns"))) {
        _turns.setRawValue(params[QStringLiteral("turns")].toDouble());
    }
    if (params.contains(QStringLiteral("altitude"))) {
        _altitude.setRawValue(params[QStringLiteral("altitude")].toDouble());
    }
    if (params.contains(QStringLiteral("direction"))) {
        _direction.setRawValue(params[QStringLiteral("direction")].toInt());
    }
    if (params.contains(QStringLiteral("flightSpeed"))) {
        _flightSpeed.setRawValue(params[QStringLiteral("flightSpeed")].toDouble());
    }
    if (params.contains(QStringLiteral("altitudeMode"))) {
        _altitudeMode.setRawValue(params[QStringLiteral("altitudeMode")].toInt());
    }
    if (params.contains(QStringLiteral("maxBankAngle"))) {
        _maxBankAngle.setRawValue(params[QStringLiteral("maxBankAngle")].toDouble());
    }
    if (params.contains(QStringLiteral("rotationAngle"))) {
        _rotationAngle.setRawValue(params[QStringLiteral("rotationAngle")].toDouble());
    }
    return true;
}
