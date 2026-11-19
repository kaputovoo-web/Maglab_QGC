/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "SquarePlanStrategy.h"
#include "PlanParameterValidation.h"

#include <QtMath>
#include <QGeoCoordinate>
#include <limits>

SquarePlanStrategy::SquarePlanStrategy(QObject* parent)
    : LLMPlanStrategy(parent)
    , _spacing     (0, QStringLiteral("spacing"),      FactMetaData::valueTypeDouble, this)
    , _legs        (0, QStringLiteral("legs"),         FactMetaData::valueTypeInt32,  this)
    , _altitude    (0, QStringLiteral("altitude"),     FactMetaData::valueTypeDouble, this)
    , _direction   (0, QStringLiteral("direction"),    FactMetaData::valueTypeInt32,  this)
    , _flightSpeed (0, QStringLiteral("flightSpeed"),  FactMetaData::valueTypeDouble, this)
    , _altitudeMode(0, QStringLiteral("altitudeMode"), FactMetaData::valueTypeInt32,  this)
    , _rotationAngle(0, QStringLiteral("rotationAngle"), FactMetaData::valueTypeDouble, this)
{
    _initMetaData();

    connect(&_spacing,      &Fact::valueChanged, this, &SquarePlanStrategy::_recomputeIfReady);
    connect(&_legs,         &Fact::valueChanged, this, &SquarePlanStrategy::_recomputeIfReady);
    connect(&_altitude,     &Fact::valueChanged, this, &SquarePlanStrategy::_recomputeIfReady);
    connect(&_direction,    &Fact::valueChanged, this, &SquarePlanStrategy::_recomputeIfReady);
    connect(&_rotationAngle, &Fact::valueChanged, this, &SquarePlanStrategy::_recomputeIfReady);
}

void SquarePlanStrategy::_initMetaData()
{
    _spacingMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _spacingMetaData->setShortDescription(tr("Spacing"));
    _spacingMetaData->setRawMin(1.0);
    _spacingMetaData->setRawMax(500.0);
    _spacingMetaData->setRawDefaultValue(25.0);
    _spacingMetaData->setDecimalPlaces(1);
    _spacingMetaData->setRawUnits("m");
    _spacing.setMetaData(_spacingMetaData, true);

    _legsMetaData = new FactMetaData(FactMetaData::valueTypeInt32, this);
    _legsMetaData->setShortDescription(tr("Legs"));
    _legsMetaData->setRawMin(2);
    _legsMetaData->setRawMax(60);
    _legsMetaData->setRawDefaultValue(12);
    _legs.setMetaData(_legsMetaData, true);

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
    _flightSpeedMetaData->setShortDescription(tr("Flight Speed"));
    _flightSpeedMetaData->setRawMin(1.0);
    _flightSpeedMetaData->setRawMax(30.0);
    _flightSpeedMetaData->setRawDefaultValue(6.0);
    _flightSpeedMetaData->setDecimalPlaces(1);
    _flightSpeedMetaData->setRawUnits("m/s");
    _flightSpeed.setMetaData(_flightSpeedMetaData, true);

    _altitudeModeMetaData = new FactMetaData(FactMetaData::valueTypeInt32, this);
    _altitudeModeMetaData->setShortDescription(tr("Altitude Mode"));
    _altitudeModeMetaData->setRawDefaultValue(0);
    _altitudeModeMetaData->setEnumInfo(QStringList{tr("Relative"), tr("AMSL"), tr("Terrain")}, {0, 1, 2});
    _altitudeMode.setMetaData(_altitudeModeMetaData, true);

    _rotationAngleMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _rotationAngleMetaData->setShortDescription(tr("Rotation"));
    _rotationAngleMetaData->setRawMin(0.0);
    _rotationAngleMetaData->setRawMax(360.0);
    _rotationAngleMetaData->setRawDefaultValue(0.0);
    _rotationAngleMetaData->setDecimalPlaces(0);
    _rotationAngleMetaData->setRawUnits(QStringLiteral("\302\260"));
    _rotationAngle.setMetaData(_rotationAngleMetaData, true);
}

QList<Fact*> SquarePlanStrategy::facts()
{
    return {&_spacing, &_legs, &_altitude, &_altitudeMode,
            &_direction, &_rotationAngle, &_flightSpeed};
}

double SquarePlanStrategy::specifiedFlightSpeed() const
{
    return _flightSpeed.rawValue().toDouble();
}

void SquarePlanStrategy::recomputeGeometry(const QGeoCoordinate& center)
{
    _lastCenter = center;
    _recomputeIfReady();
}

void SquarePlanStrategy::_recomputeIfReady()
{
    if (!_lastCenter.isValid()) { return; }

    double sp    = _spacing.rawValue().toDouble();
    int    legs  = _legs.rawValue().toInt();
    int    dir   = _direction.rawValue().toInt();
    double rot   = _rotationAngle.rawValue().toDouble();

    _path.clear();
    _complexDistance = 0.0;
    _lastItemCount = 0;

    if (sp <= 0.0 || legs < 1) {
        emit geometryChanged(); emit complexDistanceChanged(); emit lastSequenceNumberChanged();
        return;
    }

    // Unit step directions in (east, north). CW: N,E,S,W; CCW: N,W,S,E.
    static const double cwE[4] = {0.0, 1.0, 0.0, -1.0};
    static const double cwN[4] = {1.0, 0.0, -1.0, 0.0};
    static const double ccwE[4] = {0.0, -1.0, 0.0, 1.0};
    static const double ccwN[4] = {1.0, 0.0, -1.0, 0.0};
    const double* stepE = (dir == 0) ? cwE : ccwE;
    const double* stepN = (dir == 0) ? cwN : ccwN;

    // Start at the datum (centre). Each leg is subdivided into intermediate points
    // so the autopilot spline HUGS the straight legs (rounding only at the corners)
    // instead of cutting across the square.
    double ePrev = 0.0, nPrev = 0.0;
    _path.append(_lastCenter);
    QGeoCoordinate prev = _lastCenter;

    for (int i = 0; i < legs; ++i) {
        double len  = (static_cast<double>(i / 2) + 1.0) * sp;   // grows every 2 legs
        double eEnd = ePrev + stepE[i % 4] * len;
        double nEnd = nPrev + stepN[i % 4] * len;
        int segs = qBound(1, static_cast<int>(qRound(len / 8.0)), 10);  // ~8 m per sub-point
        for (int s = 1; s <= segs; ++s) {
            double f  = static_cast<double>(s) / segs;
            double ee = ePrev + (eEnd - ePrev) * f;
            double nn = nPrev + (nEnd - nPrev) * f;
            double dist = qSqrt(ee * ee + nn * nn);
            double az   = qRadiansToDegrees(qAtan2(ee, nn)) + rot;
            QGeoCoordinate pt = _lastCenter.atDistanceAndAzimuth(dist, az);
            _path.append(pt);
            _complexDistance += prev.distanceTo(pt);
            prev = pt;
        }
        ePrev = eEnd; nPrev = nEnd;
    }

    _lastItemCount = _path.size() + 1
                   + (_flightSpeed.rawValue().toDouble() > 3.0 ? 1 : 0);

    emit geometryChanged();
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged();
}

double SquarePlanStrategy::minAMSLAltitude() const { return _altitude.rawValue().toDouble(); }
double SquarePlanStrategy::maxAMSLAltitude() const { return _altitude.rawValue().toDouble(); }

QList<MissionItem*> SquarePlanStrategy::missionItems(double relAlt, MAV_FRAME frame, QObject* parent)
{
    QList<MissionItem*> items;
    if (_path.isEmpty()) {
        if (_lastCenter.isValid()) { recomputeGeometry(_lastCenter); }
        if (_path.isEmpty()) { return items; }
    }

    double speed = _flightSpeed.rawValue().toDouble();

    // Cruise speed.
    items.append(new MissionItem(0, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
                                 1, speed, -1, 0, 0,0,0, true, false, parent));

    // Brief settle pause at the start datum.
    items.append(new MissionItem(0, MAV_CMD_NAV_WAYPOINT, frame,
                                 2.0, 2.0, 0.0, 0.0,
                                 _path[0].latitude(), _path[0].longitude(), relAlt,
                                 true, false, parent));

    // Dense straight-leg waypoints retain the requested square geometry.
    for (int i = 1; i < _path.size(); ++i) {
        items.append(new MissionItem(0, MAV_CMD_NAV_WAYPOINT, frame,
                                     0.0, 0.8, 0.0, 0.0,
                                     _path[i].latitude(), _path[i].longitude(), relAlt,
                                     true, false, parent));
    }

    // Gentle end brake.
    if (speed > 3.0) {
        items.append(new MissionItem(0, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
                                     1, speed * 0.4, -1, 0, 0,0,0, true, false, parent));
    }

    _lastItemCount = items.size();
    return items;
}

QJsonObject SquarePlanStrategy::saveParams() const
{
    QJsonObject obj;
    obj[QStringLiteral("spacing")]       = _spacing.rawValue().toDouble();
    obj[QStringLiteral("legs")]          = _legs.rawValue().toInt();
    obj[QStringLiteral("altitude")]      = _altitude.rawValue().toDouble();
    obj[QStringLiteral("direction")]     = _direction.rawValue().toInt();
    obj[QStringLiteral("flightSpeed")]   = _flightSpeed.rawValue().toDouble();
    obj[QStringLiteral("altitudeMode")]  = _altitudeMode.rawValue().toInt();
    obj[QStringLiteral("rotationAngle")] = _rotationAngle.rawValue().toDouble();
    return obj;
}

bool SquarePlanStrategy::loadParams(const QJsonObject& params)
{
    if (!PlanParameterValidation::square(params)) {
        return false;
    }
    if (params.contains(QStringLiteral("spacing")))       _spacing.setRawValue(params[QStringLiteral("spacing")].toDouble());
    if (params.contains(QStringLiteral("legs")))          _legs.setRawValue(params[QStringLiteral("legs")].toInt());
    if (params.contains(QStringLiteral("altitude")))      _altitude.setRawValue(params[QStringLiteral("altitude")].toDouble());
    if (params.contains(QStringLiteral("direction")))     _direction.setRawValue(params[QStringLiteral("direction")].toInt());
    if (params.contains(QStringLiteral("flightSpeed")))   _flightSpeed.setRawValue(params[QStringLiteral("flightSpeed")].toDouble());
    if (params.contains(QStringLiteral("altitudeMode")))  _altitudeMode.setRawValue(params[QStringLiteral("altitudeMode")].toInt());
    if (params.contains(QStringLiteral("rotationAngle"))) _rotationAngle.setRawValue(params[QStringLiteral("rotationAngle")].toDouble());
    return true;
}
