/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "FomPlanStrategy.h"
#include "PlanParameterValidation.h"

#include <QtMath>
#include <QGeoCoordinate>
#include <limits>

// Local east/north offset (m) → geo coordinate around the centre.
static QGeoCoordinate localToGeo(const QGeoCoordinate& c, double e, double n)
{
    double dist = qSqrt(e * e + n * n);
    if (dist < 1e-6) { return c; }
    double az = qRadiansToDegrees(qAtan2(e, n));
    return c.atDistanceAndAzimuth(dist, az);
}

// Sample a lateral sine weave from A to B (used for the roll-disturbance edge).
static QList<QGeoCoordinate> weave(const QGeoCoordinate& A, const QGeoCoordinate& B,
                                   double amp, int cycles, int perCycle = 8)
{
    QList<QGeoCoordinate> pts;
    double len = A.distanceTo(B);
    double az  = A.azimuthTo(B);
    int total = qMax(1, cycles * perCycle);
    for (int i = 1; i <= total; ++i) {
        double t   = len * static_cast<double>(i) / total;
        double lat = amp * qSin(2.0 * M_PI * cycles * t / len);
        QGeoCoordinate along = A.atDistanceAndAzimuth(t, az);
        if (qAbs(lat) > 1e-3) {
            along = along.atDistanceAndAzimuth(qAbs(lat), az + (lat >= 0 ? 90.0 : -90.0));
        }
        pts.append(along);
    }
    return pts;
}

FomPlanStrategy::FomPlanStrategy(QObject* parent)
    : LLMPlanStrategy(parent)
    , _sideLength (0, QStringLiteral("sideLength"),   FactMetaData::valueTypeDouble, this)
    , _flightSpeed(0, QStringLiteral("flightSpeed"),  FactMetaData::valueTypeDouble, this)
    , _altitude   (0, QStringLiteral("altitude"),     FactMetaData::valueTypeDouble, this)
    , _altitudeMode(0, QStringLiteral("altitudeMode"), FactMetaData::valueTypeInt32, this)
    , _cycles     (0, QStringLiteral("cycles"),       FactMetaData::valueTypeInt32,  this)
    , _rollAmp    (0, QStringLiteral("rollAmp"),      FactMetaData::valueTypeDouble, this)
    , _rollLimit  (0, QStringLiteral("rollLimit"),    FactMetaData::valueTypeDouble, this)
    , _pitchAmp   (0, QStringLiteral("pitchAmp"),     FactMetaData::valueTypeDouble, this)
    , _pitchLimit (0, QStringLiteral("pitchLimit"),   FactMetaData::valueTypeDouble, this)
    , _yawAmp     (0, QStringLiteral("yawAmp"),       FactMetaData::valueTypeDouble, this)
    , _yawLimit   (0, QStringLiteral("yawLimit"),     FactMetaData::valueTypeDouble, this)
{
    _initMetaData();
    connect(&_sideLength,  &Fact::valueChanged, this, &FomPlanStrategy::_recomputeIfReady);
    connect(&_cycles,      &Fact::valueChanged, this, &FomPlanStrategy::_recomputeIfReady);
    connect(&_rollAmp,     &Fact::valueChanged, this, &FomPlanStrategy::_recomputeIfReady);
    connect(&_rollLimit,   &Fact::valueChanged, this, &FomPlanStrategy::_recomputeIfReady);
    connect(&_flightSpeed, &Fact::valueChanged, this, &FomPlanStrategy::_recomputeIfReady);
}

FactMetaData* FomPlanStrategy::_angleMeta(QObject* p, const QString& desc, double def)
{
    FactMetaData* m = new FactMetaData(FactMetaData::valueTypeDouble, p);
    m->setShortDescription(desc);
    m->setRawMin(0.0);
    m->setRawMax(60.0);
    m->setRawDefaultValue(def);
    m->setDecimalPlaces(0);
    m->setRawUnits(QStringLiteral("\302\260"));   // degrees
    return m;
}

void FomPlanStrategy::_initMetaData()
{
    _sideLengthMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _sideLengthMetaData->setShortDescription(tr("Side Length"));
    _sideLengthMetaData->setRawMin(5.0);
    _sideLengthMetaData->setRawMax(500.0);
    _sideLengthMetaData->setRawDefaultValue(40.0);
    _sideLengthMetaData->setDecimalPlaces(1);
    _sideLengthMetaData->setRawUnits("m");
    _sideLength.setMetaData(_sideLengthMetaData, true);

    _flightSpeedMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _flightSpeedMetaData->setShortDescription(tr("Flight Speed"));
    _flightSpeedMetaData->setRawMin(1.0);
    _flightSpeedMetaData->setRawMax(30.0);
    _flightSpeedMetaData->setRawDefaultValue(5.0);
    _flightSpeedMetaData->setDecimalPlaces(1);
    _flightSpeedMetaData->setRawUnits("m/s");
    _flightSpeed.setMetaData(_flightSpeedMetaData, true);

    _altitudeMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _altitudeMetaData->setShortDescription(tr("Altitude"));
    _altitudeMetaData->setRawMin(0.0);
    _altitudeMetaData->setRawMax(400.0);
    _altitudeMetaData->setRawDefaultValue(80.0);
    _altitudeMetaData->setDecimalPlaces(1);
    _altitudeMetaData->setRawUnits("m");
    _altitude.setMetaData(_altitudeMetaData, true);

    _altitudeModeMetaData = new FactMetaData(FactMetaData::valueTypeInt32, this);
    _altitudeModeMetaData->setShortDescription(tr("Altitude Mode"));
    _altitudeModeMetaData->setRawDefaultValue(0);
    _altitudeModeMetaData->setEnumInfo(QStringList{tr("Rel. to Takeoff"), tr("AMSL"), tr("Terrain")}, {0, 1, 2});
    _altitudeMode.setMetaData(_altitudeModeMetaData, true);

    _cyclesMetaData = new FactMetaData(FactMetaData::valueTypeInt32, this);
    _cyclesMetaData->setShortDescription(tr("Cycles / Edge"));
    _cyclesMetaData->setRawMin(1);
    _cyclesMetaData->setRawMax(10);
    _cyclesMetaData->setRawDefaultValue(3);
    _cycles.setMetaData(_cyclesMetaData, true);

    _rollAmpMeta   = _angleMeta(this, tr("Roll Amplitude"),  10.0);  _rollAmp.setMetaData(_rollAmpMeta, true);
    _rollLimitMeta = _angleMeta(this, tr("Roll Limit"),      25.0);  _rollLimit.setMetaData(_rollLimitMeta, true);
    _pitchAmpMeta  = _angleMeta(this, tr("Pitch Amplitude"), 10.0);  _pitchAmp.setMetaData(_pitchAmpMeta, true);
    _pitchLimitMeta= _angleMeta(this, tr("Pitch Limit"),     25.0);  _pitchLimit.setMetaData(_pitchLimitMeta, true);
    _yawAmpMeta    = _angleMeta(this, tr("Yaw Amplitude"),   20.0);  _yawAmp.setMetaData(_yawAmpMeta, true);
    _yawLimitMeta  = _angleMeta(this, tr("Yaw Limit"),       45.0);  _yawLimit.setMetaData(_yawLimitMeta, true);
}

QList<Fact*> FomPlanStrategy::facts()
{
    return {&_sideLength, &_flightSpeed, &_altitude, &_altitudeMode, &_cycles,
            &_rollAmp, &_rollLimit, &_pitchAmp, &_pitchLimit, &_yawAmp, &_yawLimit};
}

double FomPlanStrategy::specifiedFlightSpeed() const { return _flightSpeed.rawValue().toDouble(); }
double FomPlanStrategy::minAMSLAltitude() const { return _altitude.rawValue().toDouble(); }
double FomPlanStrategy::maxAMSLAltitude() const { return _altitude.rawValue().toDouble(); }

void FomPlanStrategy::recomputeGeometry(const QGeoCoordinate& center)
{
    _lastCenter = center;
    _recomputeIfReady();
}

void FomPlanStrategy::_recomputeIfReady()
{
    _corners.clear();
    _path.clear();
    _complexDistance = 0.0;
    _lastItemCount = 0;
    if (!_lastCenter.isValid()) { emit geometryChanged(); emit complexDistanceChanged(); emit lastSequenceNumberChanged(); return; }

    double L = _sideLength.rawValue().toDouble();
    if (L <= 0.0) { emit geometryChanged(); emit complexDistanceChanged(); emit lastSequenceNumberChanged(); return; }
    double h = L / 2.0;

    // Square corners in flight order (roll, pitch, yaw, neutral edges).
    _corners << localToGeo(_lastCenter, -h, -h)
             << localToGeo(_lastCenter,  h, -h)
             << localToGeo(_lastCenter,  h,  h)
             << localToGeo(_lastCenter, -h,  h);

    int    cycles = qMax(1, _cycles.rawValue().toInt());
    double v      = qMax(0.5, _flightSpeed.rawValue().toDouble());
    double rollDeg = qBound(0.0, qMin(_rollAmp.rawValue().toDouble(), _rollLimit.rawValue().toDouble()), 60.0);

    // Roll edge weave amplitude from physics: a = g·tanφ, and for y=A·sin(2π·cyc·x/L)
    // the peak lateral accel is A·(2π·cyc/L)²·v², so A = a·L² / ((2π·cyc)²·v²).
    double a   = kGravity * qTan(qDegreesToRadians(rollDeg));
    double rollAmpM = (rollDeg > 0.1)
        ? a * L * L / (qPow(2.0 * M_PI * cycles, 2.0) * v * v)
        : 0.0;
    rollAmpM = qBound(0.0, rollAmpM, 0.25 * L);

    // Build display path: start corner, roll-edge weave, then the other corners.
    _path << _corners[0];
    _path << weave(_corners[0], _corners[1], rollAmpM, cycles);   // roll edge
    _path << _corners[2];                                          // pitch edge (straight)
    _path << _corners[3];                                          // yaw edge (straight)
    _path << _corners[0];                                          // neutral edge back

    for (int i = 1; i < _path.size(); ++i) { _complexDistance += _path[i - 1].distanceTo(_path[i]); }

    // speed + start + roll(8/cycle) + pitch(speed+point, 2/cycle)
    // + cruise speed + yaw(command+point, 1/cycle) + neutral return.
    _lastItemCount = 4 + 14 * cycles;

    emit geometryChanged();
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged();
}

QList<MissionItem*> FomPlanStrategy::missionItems(double relAlt, MAV_FRAME frame, QObject* parent)
{
    QList<MissionItem*> items;
    if (_corners.size() < 4) {
        if (_lastCenter.isValid()) { recomputeGeometry(_lastCenter); }
        if (_corners.size() < 4) { return items; }
    }

    const double v      = qMax(0.5, _flightSpeed.rawValue().toDouble());
    const int    cycles = qMax(1, _cycles.rawValue().toInt());
    const double L      = _sideLength.rawValue().toDouble();

    auto wp = [&](MAV_CMD cmd, const QGeoCoordinate& c, double p1 = 0.0) {
        items.append(new MissionItem(0, cmd, frame, p1, (cmd == MAV_CMD_NAV_WAYPOINT ? 1.0 : 0.0),
                                     0.0, 0.0, c.latitude(), c.longitude(), relAlt, true, false, parent));
    };
    auto setSpeed = [&](double s) {
        items.append(new MissionItem(0, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
                                     1, s, -1, 0, 0, 0, 0, true, false, parent));
    };
    auto yawCmd = [&](double angle, double dir) {   // relative yaw wiggle
        items.append(new MissionItem(0, MAV_CMD_CONDITION_YAW, MAV_FRAME_MISSION,
                                     angle, 30.0 /*deg/s*/, dir, 1 /*relative*/, 0, 0, 0, true, false, parent));
    };

    setSpeed(v);
    wp(MAV_CMD_NAV_WAYPOINT, _corners[0]);

    // --- Edge 0: ROLL — lateral weave (path curvature induces bank) ---
    double rollDeg = qBound(0.0, qMin(_rollAmp.rawValue().toDouble(), _rollLimit.rawValue().toDouble()), 60.0);
    double a = kGravity * qTan(qDegreesToRadians(rollDeg));
    double rollAmpM = (rollDeg > 0.1) ? a * L * L / (qPow(2.0 * M_PI * cycles, 2.0) * v * v) : 0.0;
    rollAmpM = qBound(0.0, rollAmpM, 0.25 * L);
    const QList<QGeoCoordinate> w = weave(_corners[0], _corners[1], rollAmpM, cycles);
    for (const QGeoCoordinate& c : w) { wp(MAV_CMD_NAV_WAYPOINT, c); }

    // --- Edge 1: PITCH — fore/aft speed oscillation (accel/decel induces pitch) ---
    double pf = qBound(0.0, qMin(_pitchAmp.rawValue().toDouble(), _pitchLimit.rawValue().toDouble()) / 30.0, 0.6);
    int steps = qMax(2, cycles * 2);
    for (int i = 1; i <= steps; ++i) {
        double s = (i % 2 == 1) ? v * (1.0 + pf) : qMax(1.0, v * (1.0 - pf));
        setSpeed(s);
        double frac = static_cast<double>(i) / steps;
        QGeoCoordinate c = _corners[1].atDistanceAndAzimuth(
            _corners[1].distanceTo(_corners[2]) * frac, _corners[1].azimuthTo(_corners[2]));
        wp(MAV_CMD_NAV_WAYPOINT, c);
    }
    setSpeed(v);

    // --- Edge 2: YAW — real CONDITION_YAW heading wiggle ---
    double yawDeg = qBound(0.0, qMin(_yawAmp.rawValue().toDouble(), _yawLimit.rawValue().toDouble()), 60.0);
    for (int i = 1; i <= cycles; ++i) {
        yawCmd(yawDeg, (i % 2 == 1) ? 1.0 : -1.0);   // alternate direction
        double frac = static_cast<double>(i) / cycles;
        QGeoCoordinate c = _corners[2].atDistanceAndAzimuth(
            _corners[2].distanceTo(_corners[3]) * frac, _corners[2].azimuthTo(_corners[3]));
        wp(MAV_CMD_NAV_WAYPOINT, c);
    }

    // --- Edge 3: NEUTRAL — straight back to start ---
    wp(MAV_CMD_NAV_WAYPOINT, _corners[0]);

    _lastItemCount = items.size();
    return items;
}

QJsonObject FomPlanStrategy::saveParams() const
{
    QJsonObject o;
    o[QStringLiteral("sideLength")]   = _sideLength.rawValue().toDouble();
    o[QStringLiteral("flightSpeed")]  = _flightSpeed.rawValue().toDouble();
    o[QStringLiteral("altitude")]     = _altitude.rawValue().toDouble();
    o[QStringLiteral("altitudeMode")] = _altitudeMode.rawValue().toInt();
    o[QStringLiteral("cycles")]       = _cycles.rawValue().toInt();
    o[QStringLiteral("rollAmp")]      = _rollAmp.rawValue().toDouble();
    o[QStringLiteral("rollLimit")]    = _rollLimit.rawValue().toDouble();
    o[QStringLiteral("pitchAmp")]     = _pitchAmp.rawValue().toDouble();
    o[QStringLiteral("pitchLimit")]   = _pitchLimit.rawValue().toDouble();
    o[QStringLiteral("yawAmp")]       = _yawAmp.rawValue().toDouble();
    o[QStringLiteral("yawLimit")]     = _yawLimit.rawValue().toDouble();
    return o;
}

bool FomPlanStrategy::loadParams(const QJsonObject& p)
{
    if (!PlanParameterValidation::fom(p)) {
        return false;
    }
    auto setD = [&](const char* k, Fact& f){ if (p.contains(QLatin1String(k))) f.setRawValue(p[QLatin1String(k)].toDouble()); };
    auto setI = [&](const char* k, Fact& f){ if (p.contains(QLatin1String(k))) f.setRawValue(p[QLatin1String(k)].toInt()); };
    if (p.contains(QStringLiteral("sideLength"))) {
        setD("sideLength", _sideLength);
    } else if (p.contains(QStringLiteral("radius"))) {
        _sideLength.setRawValue(p[QStringLiteral("radius")].toDouble() * 2.0);
    }
    setD("flightSpeed", _flightSpeed);  setD("altitude", _altitude);
    setI("altitudeMode", _altitudeMode);
    if (p.contains(QStringLiteral("cycles"))) {
        setI("cycles", _cycles);
    } else if (p.contains(QStringLiteral("loops"))) {
        _cycles.setRawValue(p[QStringLiteral("loops")].toInt());
    }
    setD("rollAmp", _rollAmp);  setD("rollLimit", _rollLimit);
    setD("pitchAmp", _pitchAmp); setD("pitchLimit", _pitchLimit);
    setD("yawAmp", _yawAmp);    setD("yawLimit", _yawLimit);
    return true;
}
