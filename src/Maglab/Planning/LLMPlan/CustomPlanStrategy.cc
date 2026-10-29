/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "CustomPlanStrategy.h"

#include <QtMath>
#include <QGeoCoordinate>
#include <QJsonArray>
#include <limits>
#include <cmath>

CustomPlanStrategy::CustomPlanStrategy(QObject* parent)
    : LLMPlanStrategy(parent)
    , _altitude    (0, QStringLiteral("altitude"),     FactMetaData::valueTypeDouble, this)
    , _flightSpeed (0, QStringLiteral("flightSpeed"),  FactMetaData::valueTypeDouble, this)
    , _altitudeMode(0, QStringLiteral("altitudeMode"), FactMetaData::valueTypeInt32,  this)
{
    _initMetaData();
}

void CustomPlanStrategy::_initMetaData()
{
    _altitudeMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _altitudeMetaData->setShortDescription(tr("Altitude"));
    _altitudeMetaData->setRawMin(0.0);
    _altitudeMetaData->setRawMax(400.0);
    _altitudeMetaData->setRawDefaultValue(80.0);
    _altitudeMetaData->setDecimalPlaces(1);
    _altitudeMetaData->setRawUnits("m");
    _altitude.setMetaData(_altitudeMetaData, true);

    _flightSpeedMetaData = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _flightSpeedMetaData->setShortDescription(tr("Flight Speed"));
    _flightSpeedMetaData->setRawMin(1.0);
    _flightSpeedMetaData->setRawMax(30.0);
    _flightSpeedMetaData->setRawDefaultValue(5.0);
    _flightSpeedMetaData->setDecimalPlaces(1);
    _flightSpeedMetaData->setRawUnits("m/s");
    _flightSpeed.setMetaData(_flightSpeedMetaData, true);

    _altitudeModeMetaData = new FactMetaData(FactMetaData::valueTypeInt32, this);
    _altitudeModeMetaData->setShortDescription(tr("Altitude Mode"));
    _altitudeModeMetaData->setRawDefaultValue(0);
    _altitudeModeMetaData->setEnumInfo(QStringList{tr("Relative"), tr("AMSL"), tr("Terrain")}, {0, 1, 2});
    _altitudeMode.setMetaData(_altitudeModeMetaData, true);
}

double CustomPlanStrategy::specifiedFlightSpeed() const
{
    return _flightSpeed.rawValue().toDouble();
}

void CustomPlanStrategy::setOffsets(const QList<QPointF>& offsets)
{
    _offsets = offsets;
}

// Convert east/north offsets (meters) to WGS84 coordinates
void CustomPlanStrategy::recomputeGeometry(const QGeoCoordinate& center)
{
    _lastCenter = center;
    if (!center.isValid() || _offsets.isEmpty()) {
        _path.clear();
        _complexDistance = 0.0;
        emit geometryChanged();
        emit complexDistanceChanged();
        emit lastSequenceNumberChanged();
        return;
    }

    _path.clear();
    _path.reserve(_offsets.size() + (_closeLoop ? 1 : 0));
    _complexDistance = 0.0;

    QGeoCoordinate prevPt;
    for (int i = 0; i < _offsets.size(); ++i) {
        double east  = _offsets[i].x();   // positive = east
        double north = _offsets[i].y();   // positive = north

        double distance  = qSqrt(east * east + north * north);
        double azimuth   = qRadiansToDegrees(qAtan2(east, north));
        if (azimuth < 0.0) { azimuth += 360.0; }

        QGeoCoordinate pt = center.atDistanceAndAzimuth(distance, azimuth);
        _path.append(pt);

        if (i > 0) {
            _complexDistance += prevPt.distanceTo(pt);
        }
        prevPt = pt;
    }

    // Keep the displayed route and emitted mission identical.
    if (_closeLoop && _path.size() >= 2 && _path.last().distanceTo(_path.first()) >= 0.1) {
        _complexDistance += _path.last().distanceTo(_path.first());
        _path.append(_path.first());
    }

    emit geometryChanged();
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged();
}

QList<MissionItem*> CustomPlanStrategy::missionItems(double relAlt, MAV_FRAME frame, QObject* parent)
{
    QList<MissionItem*> items;
    if (_path.isEmpty()) { return items; }

    // DO_CHANGE_SPEED
    MissionItem* speedItem = new MissionItem(
        0, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
        1, _flightSpeed.rawValue().toDouble(), -1, 0, 0,0,0,
        true, false, parent);
    items.append(speedItem);

    for (int i = 0; i < _path.size(); ++i) {
        MissionItem* mi = new MissionItem(
            0, MAV_CMD_NAV_WAYPOINT, frame,
            0.0, 0.0, 0.0,
            0.0,
            _path[i].latitude(), _path[i].longitude(), relAlt,
            true, false, parent);
        items.append(mi);
    }


    return items;
}

QJsonObject CustomPlanStrategy::saveParams() const
{
    QJsonObject obj;
    obj[QStringLiteral("altitude")]     = _altitude.rawValue().toDouble();
    obj[QStringLiteral("altitudeMode")] = _altitudeMode.rawValue().toInt();
    obj[QStringLiteral("flightSpeed")]  = _flightSpeed.rawValue().toDouble();
    obj[QStringLiteral("closeLoop")]    = _closeLoop;

    QJsonArray arr;
    for (const QPointF& off : _offsets) {
        QJsonObject o;
        o[QStringLiteral("e")] = off.x();
        o[QStringLiteral("n")] = off.y();
        arr.append(o);
    }
    obj[QStringLiteral("offsets")] = arr;

    return obj;
}

bool CustomPlanStrategy::loadParams(const QJsonObject& params)
{
    // Validate the complete input before changing any Facts or geometry.
    // Missing/non-numeric AI offsets must not silently become (0, 0).
    for (const auto *key : {"altitude", "flightSpeed", "altitudeMode"}) {
        const QJsonValue value = params.value(QLatin1String(key));
        if (!value.isUndefined() && (!value.isDouble() || !std::isfinite(value.toDouble()))) {
            return false;
        }
    }
    if (params.contains(QStringLiteral("flightSpeed")) &&
            params.value(QStringLiteral("flightSpeed")).toDouble() <= 0.0) {
        return false;
    }
    if (params.contains(QStringLiteral("altitudeMode"))) {
        const double mode = params.value(QStringLiteral("altitudeMode")).toDouble();
        if (mode != 0.0 && mode != 1.0 && mode != 2.0) {
            return false;
        }
    }
    if (params.contains(QStringLiteral("closeLoop")) &&
            !params.value(QStringLiteral("closeLoop")).isBool()) {
        return false;
    }
    if (params.contains(QStringLiteral("offsets"))) {
        const QJsonValue offsets = params.value(QStringLiteral("offsets"));
        if (!offsets.isArray()) {
            return false;
        }
        for (const QJsonValue& value : offsets.toArray()) {
            if (!value.isObject()) {
                return false;
            }
            const QJsonObject offset = value.toObject();
            for (const auto *key : {"e", "n"}) {
                const QJsonValue coordinate = offset.value(QLatin1String(key));
                if (!coordinate.isDouble() || !std::isfinite(coordinate.toDouble())) {
                    return false;
                }
            }
        }
    }

    if (params.contains(QStringLiteral("altitude"))) {
        _altitude.setRawValue(params[QStringLiteral("altitude")].toDouble());
    }
    if (params.contains(QStringLiteral("flightSpeed"))) {
        _flightSpeed.setRawValue(params[QStringLiteral("flightSpeed")].toDouble());
    }
    if (params.contains(QStringLiteral("altitudeMode"))) {
        _altitudeMode.setRawValue(params[QStringLiteral("altitudeMode")].toInt());
    }
    if (params.contains(QStringLiteral("closeLoop"))) {
        _closeLoop = params[QStringLiteral("closeLoop")].toBool();
    }

    if (params.contains(QStringLiteral("offsets"))) {
        _offsets.clear();
        QJsonArray arr = params[QStringLiteral("offsets")].toArray();
        for (const QJsonValue& val : arr) {
            QJsonObject o = val.toObject();
            _offsets.append(QPointF(o[QStringLiteral("e")].toDouble(),
                                     o[QStringLiteral("n")].toDouble()));
        }
    }

    return true;
}
