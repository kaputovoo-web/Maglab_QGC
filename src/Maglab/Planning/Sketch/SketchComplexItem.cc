/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "SketchComplexItem.h"
#include "PlanMasterController.h"
#include "MissionController.h"
#include "SurveyComplexItem.h"
#include "CameraCalc.h"
#include "QGCMapPolygon.h"
#include "QGCApplication.h"
#include "Vehicle.h"

#include <QtMath>
#include <QJsonArray>
#include <QTimer>
#include <QPointer>
#include <limits>
#include <algorithm>
#include <cmath>

QGC_LOGGING_CATEGORY(SketchComplexItemLog, "SketchComplexItemLog")

const QString SketchComplexItem::name(QStringLiteral("Sketch"));
const char* SketchComplexItem::jsonComplexItemTypeValue = "Sketch";

SketchComplexItem::SketchComplexItem(PlanMasterController* masterController, bool flyView, const QString& kmlFile)
    : ComplexMissionItem(masterController, flyView)
    , _altitude    (0, QStringLiteral("altitude"),     FactMetaData::valueTypeDouble, this)
    , _altitudeMode(0, QStringLiteral("altitudeMode"), FactMetaData::valueTypeInt32,  this)
    , _flightSpeed (0, QStringLiteral("flightSpeed"),  FactMetaData::valueTypeDouble, this)
    , _gridSpacing (0, QStringLiteral("gridSpacing"),  FactMetaData::valueTypeDouble, this)
{
    Q_UNUSED(kmlFile);
    _editorQml = QStringLiteral("qrc:/maglab/SketchItemEditor.qml");

    _altitudeMeta = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _altitudeMeta->setShortDescription(tr("Altitude")); _altitudeMeta->setRawMin(0.0); _altitudeMeta->setRawMax(400.0);
    _altitudeMeta->setRawDefaultValue(80.0); _altitudeMeta->setDecimalPlaces(1); _altitudeMeta->setRawUnits("m");
    _altitude.setMetaData(_altitudeMeta, true);

    _flightSpeedMeta = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _flightSpeedMeta->setShortDescription(tr("Flight Speed")); _flightSpeedMeta->setRawMin(1.0); _flightSpeedMeta->setRawMax(30.0);
    _flightSpeedMeta->setRawDefaultValue(5.0); _flightSpeedMeta->setDecimalPlaces(1); _flightSpeedMeta->setRawUnits("m/s");
    _flightSpeed.setMetaData(_flightSpeedMeta, true);

    _gridSpacingMeta = new FactMetaData(FactMetaData::valueTypeDouble, this);
    _gridSpacingMeta->setShortDescription(tr("Grid Spacing")); _gridSpacingMeta->setRawMin(1.0); _gridSpacingMeta->setRawMax(200.0);
    _gridSpacingMeta->setRawDefaultValue(30.0); _gridSpacingMeta->setDecimalPlaces(1); _gridSpacingMeta->setRawUnits("m");
    _gridSpacing.setMetaData(_gridSpacingMeta, true);

    _altitudeModeMeta = new FactMetaData(FactMetaData::valueTypeInt32, this);
    _altitudeModeMeta->setShortDescription(tr("Altitude Mode"));
    _altitudeModeMeta->setRawDefaultValue(0);
    _altitudeModeMeta->setEnumInfo(QStringList{tr("Rel. to Takeoff"), tr("AMSL"), tr("Terrain")}, {0, 1, 2});
    _altitudeMode.setMetaData(_altitudeModeMeta, true);

    connect(&_altitude,    &Fact::valueChanged, this, [this](QVariant){ setDirty(true); emit pathChanged(); });
    connect(&_flightSpeed, &Fact::valueChanged, this, [this](QVariant){ setDirty(true); });
}

QVariantList SketchComplexItem::path() const
{
    QVariantList l;
    for (const QGeoCoordinate& c : _points) { l.append(QVariant::fromValue(c)); }
    return l;
}

QGeoCoordinate SketchComplexItem::coordinate() const
{
    if (!_points.isEmpty()) { return _points.first(); }
    return _coordinate;   // placement anchor before anything is drawn
}
QGeoCoordinate SketchComplexItem::entryCoordinate() const
{
    return coordinate();
}

QGeoCoordinate SketchComplexItem::exitCoordinate() const
{
    return _points.isEmpty() ? QGeoCoordinate() : _points.last();
}

void SketchComplexItem::setDrawMode(int mode)
{
    if (_drawMode != mode) { _drawMode = mode; clearPoints(); emit drawModeChanged(_drawMode); setDirty(true); }
}
void SketchComplexItem::setRouteStyle(int style)
{
    if (_routeStyle != style) { _routeStyle = style; emit routeStyleChanged(_routeStyle); emit pathChanged(); setDirty(true); }
}
void SketchComplexItem::setDrawing(bool drawing)
{
    if (_drawing != drawing) { _drawing = drawing; emit drawingChanged(_drawing); }
}
void SketchComplexItem::setDrawArmed(bool armed)
{
    if (_drawArmed != armed) { _drawArmed = armed; emit drawArmedChanged(_drawArmed); }
}

void SketchComplexItem::beginStroke()
{
    _points.clear(); _ptStyle.clear();
    setDrawing(true);
    emit pathChanged();
}

void SketchComplexItem::addPoint(const QGeoCoordinate& c)
{
    if (!c.isValid()) { return; }
    // Thin while drawing: skip points closer than ~2 m to the previous one.
    if (!_points.isEmpty() && _points.last().distanceTo(c) < 2.0) { return; }
    _points.append(c);
    _ptStyle.append(-1);
    emit pathChanged();
}

void SketchComplexItem::endStroke()
{
    setDrawing(false);
    // Douglas–Peucker simplify so a freehand drag becomes a sensible set of waypoints.
    double eps = qMax(3.0, _gridSpacing.rawValue().toDouble() * 0.15);
    QList<QGeoCoordinate> simp = _simplify(_points, eps);
    if (simp.size() >= 2) {
        _points = simp;
        _ptStyle = QList<int>(); for (int i = 0; i < _points.size(); ++i) { _ptStyle.append(-1); }
    }
    _rebuild();
}

void SketchComplexItem::clearPoints()
{
    _points.clear(); _ptStyle.clear(); _complexDistance = 0.0;
    emit pathChanged(); setDirty(true);
}

void SketchComplexItem::setPointStyle(int index, int style)
{
    if (index >= 0 && index < _ptStyle.size()) { _ptStyle[index] = style; emit pathChanged(); setDirty(true); }
}
int SketchComplexItem::pointStyle(int index) const
{
    return (index >= 0 && index < _ptStyle.size()) ? _ptStyle[index] : -1;
}
void SketchComplexItem::setSelectedIndex(int i)
{
    if (_selectedIndex != i) { _selectedIndex = i; emit selectedIndexChanged(_selectedIndex); }
}
void SketchComplexItem::movePoint(int index, const QGeoCoordinate& c)
{
    if (index >= 0 && index < _points.size() && c.isValid()) { _points[index] = c; _rebuild(); }
}
void SketchComplexItem::removePoint(int index)
{
    if (index < 0 || index >= _points.size()) { return; }
    _points.removeAt(index);
    if (index < _ptStyle.size()) { _ptStyle.removeAt(index); }
    if (_selectedIndex >= _points.size()) { setSelectedIndex(_points.size() - 1); }
    _rebuild();
}

// --- Minimum-area oriented bounding rectangle (consolidate region → 4 corners) ---
namespace {
struct Pt { double x, y; };
double cross(const Pt& o, const Pt& a, const Pt& b)
{ return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

QVector<Pt> convexHull(QVector<Pt> p)
{
    std::sort(p.begin(), p.end(), [](const Pt& a, const Pt& b){ return a.x < b.x || (a.x == b.x && a.y < b.y); });
    int n = p.size(), k = 0;
    if (n < 3) { return p; }
    QVector<Pt> h(2 * n);
    for (int i = 0; i < n; ++i) { while (k >= 2 && cross(h[k-2], h[k-1], p[i]) <= 0) k--; h[k++] = p[i]; }
    for (int i = n - 2, t = k + 1; i >= 0; --i) { while (k >= t && cross(h[k-2], h[k-1], p[i]) <= 0) k--; h[k++] = p[i]; }
    h.resize(k - 1);
    return h;
}
} // namespace

void SketchComplexItem::makeQuad()
{
    if (_points.size() < 3) { return; }

    // Reference = centroid; project to local east/north.
    double lat0 = 0, lon0 = 0;
    for (const QGeoCoordinate& c : _points) { lat0 += c.latitude(); lon0 += c.longitude(); }
    lat0 /= _points.size(); lon0 /= _points.size();
    QGeoCoordinate ref(lat0, lon0);

    QVector<Pt> pts;
    for (const QGeoCoordinate& c : _points) {
        double d = ref.distanceTo(c), a = qDegreesToRadians(ref.azimuthTo(c));
        pts.append({ d * qSin(a), d * qCos(a) });   // (east, north)
    }
    QVector<Pt> hull = convexHull(pts);
    if (hull.size() < 3) { return; }

    // Rotating calipers: try each hull edge as a rectangle axis, keep min area.
    double bestArea = std::numeric_limits<double>::max();
    Pt bestC[4];
    for (int i = 0; i < hull.size(); ++i) {
        Pt e0 = hull[i], e1 = hull[(i + 1) % hull.size()];
        double ex = e1.x - e0.x, ey = e1.y - e0.y;
        double len = std::hypot(ex, ey); if (len < 1e-6) { continue; }
        ex /= len; ey /= len;                 // edge unit; normal = (-ey, ex)
        double minU = 1e18, maxU = -1e18, minV = 1e18, maxV = -1e18;
        for (const Pt& h : hull) {
            double u = h.x * ex + h.y * ey;
            double v = -h.x * ey + h.y * ex;
            minU = qMin(minU, u); maxU = qMax(maxU, u);
            minV = qMin(minV, v); maxV = qMax(maxV, v);
        }
        double area = (maxU - minU) * (maxV - minV);
        if (area < bestArea) {
            bestArea = area;
            // corners in (u,v) → back to (x,y): p = u*edge + v*normal
            double uv[4][2] = {{minU,minV},{maxU,minV},{maxU,maxV},{minU,maxV}};
            for (int k = 0; k < 4; ++k) {
                double u = uv[k][0], v = uv[k][1];
                bestC[k] = { u * ex - v * ey, u * ey + v * ex };
            }
        }
    }

    _points.clear(); _ptStyle.clear();
    for (int k = 0; k < 4; ++k) {
        double d = std::hypot(bestC[k].x, bestC[k].y);
        double az = qRadiansToDegrees(qAtan2(bestC[k].x, bestC[k].y));
        _points.append(ref.atDistanceAndAzimuth(d, az));
        _ptStyle.append(-1);
    }
    setSelectedIndex(-1);
    _rebuild();
}
int SketchComplexItem::_effectiveStyle(int index) const
{
    int s = pointStyle(index);
    return (s < 0) ? _routeStyle : s;
}

void SketchComplexItem::_rebuild()
{
    _complexDistance = 0.0;
    for (int i = 1; i < _points.size(); ++i) { _complexDistance += _points[i - 1].distanceTo(_points[i]); }
    emit pathChanged();
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged(lastSequenceNumber());
    setDirty(true);
}

// --- Douglas–Peucker on geographic points (local planar approx via distanceTo). ---
QList<QGeoCoordinate> SketchComplexItem::_simplify(const QList<QGeoCoordinate>& pts, double epsM)
{
    if (pts.size() < 3) { return pts; }
    int end = pts.size() - 1;
    double dmax = 0.0; int idx = 0;
    const QGeoCoordinate& a = pts.first();
    const QGeoCoordinate& b = pts.last();
    double ab = a.distanceTo(b);
    for (int i = 1; i < end; ++i) {
        double d;
        if (ab < 1e-3) {
            d = a.distanceTo(pts[i]);
        } else {
            // Perpendicular distance via triangle area: area = 0.5|AB|·h.
            double ai = a.distanceTo(pts[i]);
            double bi = b.distanceTo(pts[i]);
            double s  = 0.5 * (ab + ai + bi);
            double area = qSqrt(qMax(0.0, s * (s - ab) * (s - ai) * (s - bi)));
            d = 2.0 * area / ab;
        }
        if (d > dmax) { dmax = d; idx = i; }
    }
    QList<QGeoCoordinate> out;
    if (dmax > epsM) {
        QList<QGeoCoordinate> left  = _simplify(pts.mid(0, idx + 1), epsM);
        QList<QGeoCoordinate> right = _simplify(pts.mid(idx), epsM);
        out = left;
        out.removeLast();
        out.append(right);
    } else {
        out << a << b;
    }
    return out;
}

void SketchComplexItem::generateSurvey()
{
    QString err;
    if (!finishSurvey(err)) {
        qgcApp()->showAppMessage(err);
    }
}

bool SketchComplexItem::finishSurvey(QString& errorString)
{
    if (_missionController == nullptr) { errorString = tr("No mission controller."); return false; }
    if (_points.size() < 3) { errorString = tr("Draw a closed area with at least 3 points first."); return false; }

    QGeoCoordinate center = coordinate();
    // Insert the Survey at THIS item's slot so it replaces the Sketch in place
    // (between Takeoff and Return/Land), not appended after Return-to-Launch.
    int insertIndex = _missionController->visualItems()
                    ? _missionController->visualItems()->indexOf(this) : -1;
    if (insertIndex < 0) {
        errorString = tr("The Sketch item is no longer part of this mission.");
        return false;
    }
    VisualMissionItem* vmi = _missionController->insertComplexMissionItem(
        QString::fromUtf8(SurveyComplexItem::canonicalName), center, insertIndex, true /* makeCurrentItem */);
    SurveyComplexItem* survey = qobject_cast<SurveyComplexItem*>(vmi);
    if (survey == nullptr) { errorString = tr("Failed to create native Survey."); return false; }

    QGCMapPolygon* poly = survey->surveyAreaPolygon();
    poly->clear();
    poly->appendVertices(_points);

    CameraCalc* cc = survey->cameraCalc();
    cc->setCameraBrand(CameraCalc::canonicalManualCameraName());
    cc->valueSetIsDistance()->setRawValue(true);
    cc->distanceToSurface()->setRawValue(_altitude.rawValue().toDouble());
    cc->adjustedFootprintSide()->setRawValue(_gridSpacing.rawValue().toDouble());

    // Remove this Sketch console item; the native Survey is the real task now.
    MissionController* mc = _missionController;
    const QPointer<SketchComplexItem> self(this);
    QTimer::singleShot(0, mc, [mc, self]() {
        if (!self || mc->visualItems() == nullptr) { return; }
        int idx = mc->visualItems()->indexOf(self.data());
        if (idx >= 0) { mc->removeVisualItem(idx); }
    });
    return true;
}

void SketchComplexItem::appendMissionItems(QList<MissionItem*>& items, QObject* missionItemParent)
{
    if (_drawMode == SurveyArea) { return; }   // survey is a separate spawned item
    if (_points.isEmpty()) { return; }

    double relAlt = _altitude.rawValue().toDouble();
    double speed  = _flightSpeed.rawValue().toDouble();
    int seq = _sequenceNumber;

    int mode = _altitudeMode.rawValue().toInt();
    MAV_FRAME frame = (mode == 1) ? MAV_FRAME_GLOBAL
                    : (mode == 2) ? MAV_FRAME_GLOBAL_TERRAIN_ALT
                                  : MAV_FRAME_GLOBAL_RELATIVE_ALT;

    items.append(new MissionItem(seq++, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
                                 1, speed, -1, 0, 0,0,0, true, false, missionItemParent));
    const int n = _points.size();
    for (int i = 0; i < n; ++i) {
        bool curve = (_effectiveStyle(i) == 1) && (i != 0) && (i != n - 1);  // user-selected curved path
        MAV_CMD cmd = curve ? MAV_CMD_NAV_SPLINE_WAYPOINT : MAV_CMD_NAV_WAYPOINT;
        items.append(new MissionItem(seq++, cmd, frame,
                                     0.0, (cmd == MAV_CMD_NAV_WAYPOINT ? 1.0 : 0.0), 0.0, 0.0,
                                     _points[i].latitude(), _points[i].longitude(), relAlt,
                                     true, false, missionItemParent));
    }

    // Restore default speed so this hand-drawn route does not change following Survey/RTL legs.
    double restoreSpeed = 5.0;
    if (_controllerVehicle != nullptr) {
        restoreSpeed = (_controllerVehicle->multiRotor() || _controllerVehicle->vtol())
                     ? _controllerVehicle->defaultHoverSpeed()
                     : _controllerVehicle->defaultCruiseSpeed();
    }
    if (!(restoreSpeed > 0.0)) {
        restoreSpeed = 5.0;
    }
    items.append(new MissionItem(seq++, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
                                 1, restoreSpeed, -1, 0, 0, 0, 0,
                                 true, false, missionItemParent));
}

int SketchComplexItem::lastSequenceNumber() const
{
    if (_drawMode == SurveyArea || _points.isEmpty()) { return _sequenceNumber; }
    return _sequenceNumber + _points.size() + 1;   // speed cmd + points + restore speed
}

void SketchComplexItem::setMissionFlightStatus(MissionFlightStatus_t& missionFlightStatus)
{
    ComplexMissionItem::setMissionFlightStatus(missionFlightStatus);
}

void SketchComplexItem::applyNewAltitude(double newAltitude)
{
    _altitude.setRawValue(newAltitude);
}

VisualMissionItem::ReadyForSaveState SketchComplexItem::readyForSaveState() const
{
    // A drawn region is only an editing aid until converted to native Survey.
    // appendMissionItems emits no commands for SurveyArea, so it must not be
    // treated as an upload-ready mission merely because three points exist.
    if (_drawMode == SurveyArea) {
        return NotReadyForSaveData;
    }
    const double altitude = _altitude.rawValue().toDouble();
    const double speed = _flightSpeed.rawValue().toDouble();
    const int altitudeMode = _altitudeMode.rawValue().toInt();
    if (!std::isfinite(altitude) || !std::isfinite(speed) || speed <= 0.0 ||
            altitudeMode < 0 || altitudeMode > 2) {
        return NotReadyForSaveData;
    }
    return _points.size() >= 2 ? ReadyForSave : NotReadyForSaveData;
}

double SketchComplexItem::greatestDistanceTo(const QGeoCoordinate& other) const
{
    double d = 0.0;
    for (const QGeoCoordinate& c : _points) { d = qMax(d, other.distanceTo(c)); }
    return d;
}

void SketchComplexItem::setCoordinate(const QGeoCoordinate& coordinate)
{
    if (!coordinate.isValid()) { return; }

    // Nothing drawn yet: remember where the item was placed so it has a real
    // position (and stays a coordinate parent) until the first stroke.
    if (_points.isEmpty()) {
        if (_coordinate == coordinate) { return; }
        _coordinate = coordinate;
        emit coordinateChanged(_coordinate);
        setDirty(true);
        return;
    }

    QGeoCoordinate cur = this->coordinate();
    if (!cur.isValid()) { return; }
    double az = cur.azimuthTo(coordinate);
    double dist = cur.distanceTo(coordinate);
    if (dist < 0.01) { return; }
    for (int i = 0; i < _points.size(); ++i) { _points[i] = _points[i].atDistanceAndAzimuth(dist, az); }
    emit pathChanged();
    emit coordinateChanged(_points.first());
    setDirty(true);
}

void SketchComplexItem::setDirty(bool dirty)
{
    if (_dirty != dirty) { _dirty = dirty; emit dirtyChanged(_dirty); }
}

void SketchComplexItem::setSequenceNumber(int sequenceNumber)
{
    if (_sequenceNumber != sequenceNumber) {
        _sequenceNumber = sequenceNumber;
        emit sequenceNumberChanged(_sequenceNumber);
        emit lastSequenceNumberChanged(lastSequenceNumber());
    }
}

void SketchComplexItem::save(QJsonArray& missionItems)
{
    QJsonObject o;
    o[QStringLiteral("type")]            = VisualMissionItem::jsonTypeComplexItemValue;
    o[QStringLiteral("complexItemType")] = QString::fromLatin1(jsonComplexItemTypeValue);
    o[QStringLiteral("version")]         = 1;
    o[QStringLiteral("drawMode")]     = _drawMode;
    o[QStringLiteral("routeStyle")]   = _routeStyle;
    o[QStringLiteral("altitude")]     = _altitude.rawValue().toDouble();
    o[QStringLiteral("altitudeMode")] = _altitudeMode.rawValue().toInt();
    o[QStringLiteral("flightSpeed")]  = _flightSpeed.rawValue().toDouble();
    o[QStringLiteral("gridSpacing")]  = _gridSpacing.rawValue().toDouble();
    if (_coordinate.isValid()) {
        QJsonObject c;
        c[QStringLiteral("lat")] = _coordinate.latitude();
        c[QStringLiteral("lon")] = _coordinate.longitude();
        o[QStringLiteral("center")] = c;
    }
    QJsonArray pts;
    for (int i = 0; i < _points.size(); ++i) {
        QJsonObject p;
        p[QStringLiteral("lat")] = _points[i].latitude();
        p[QStringLiteral("lon")] = _points[i].longitude();
        p[QStringLiteral("st")]  = (i < _ptStyle.size() ? _ptStyle[i] : -1);
        pts.append(p);
    }
    o[QStringLiteral("points")] = pts;
    missionItems.append(o);
}

bool SketchComplexItem::load(const QJsonObject& complexObject, int sequenceNumber, QString& errorString)
{
    const int version = complexObject[QStringLiteral("version")].toInt(-1);
    const int drawMode = complexObject[QStringLiteral("drawMode")].toInt(-1);
    const int routeStyle = complexObject[QStringLiteral("routeStyle")].toInt(-1);
    if (version != 1 || drawMode < SurveyArea || drawMode > Waypoints ||
            routeStyle < 0 || routeStyle > 1 ||
            !complexObject.value(QStringLiteral("points")).isArray()) {
        errorString = tr("Invalid or unsupported Sketch complex item.");
        return false;
    }

    _sequenceNumber = sequenceNumber;
    _drawMode   = drawMode;
    _routeStyle = routeStyle;
    _altitude.setRawValue(complexObject[QStringLiteral("altitude")].toDouble(80.0));
    _altitudeMode.setRawValue(complexObject[QStringLiteral("altitudeMode")].toInt(0));
    _flightSpeed.setRawValue(complexObject[QStringLiteral("flightSpeed")].toDouble(5.0));
    _gridSpacing.setRawValue(complexObject[QStringLiteral("gridSpacing")].toDouble(30.0));
    // Placement anchor (absent in missions written before this field existed)
    _coordinate = QGeoCoordinate();
    const QJsonValue centerValue = complexObject.value(QStringLiteral("center"));
    if (centerValue.isObject()) {
        const QJsonObject c = centerValue.toObject();
        if (c.value(QStringLiteral("lat")).isDouble() && c.value(QStringLiteral("lon")).isDouble()) {
            const QGeoCoordinate anchor(c[QStringLiteral("lat")].toDouble(),
                                        c[QStringLiteral("lon")].toDouble());
            if (anchor.isValid()) { _coordinate = anchor; }
        }
    }
    _points.clear(); _ptStyle.clear();
    for (const QJsonValue& v : complexObject[QStringLiteral("points")].toArray()) {
        if (!v.isObject()) {
            errorString = tr("Sketch point is not an object.");
            return false;
        }
        const QJsonObject p = v.toObject();
        if (!p.value(QStringLiteral("lat")).isDouble() ||
                !p.value(QStringLiteral("lon")).isDouble()) {
            errorString = tr("Sketch point is missing latitude or longitude.");
            return false;
        }
        const QGeoCoordinate point(p[QStringLiteral("lat")].toDouble(),
                                   p[QStringLiteral("lon")].toDouble());
        if (!point.isValid()) {
            errorString = tr("Sketch contains an invalid coordinate.");
            return false;
        }
        _points.append(point);
        _ptStyle.append(qBound(-1, p[QStringLiteral("st")].toInt(-1), 1));
    }
    _rebuild();
    return true;
}
