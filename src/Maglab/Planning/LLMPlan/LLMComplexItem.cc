/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "LLMComplexItem.h"
#include "LLMPlanStrategy.h"
#include "VoicePlanStrategy.h"
#include "SpiralPlanStrategy.h"
#include "FomPlanStrategy.h"
#include "CustomPlanStrategy.h"
#include "SquarePlanStrategy.h"
#include "JsonParsing.h"
#include "MissionController.h"
#include "QGCQGeoCoordinate.h"
#include "PlanMasterController.h"
#include "FlightPathSegment.h"
#include "MAVLinkProtocol.h"
#include "Vehicle.h"
#include "LinkInterface.h"
#include "SurveyComplexItem.h"
#include "CameraCalc.h"
#include "QmlObjectListModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <cmath>

QGC_LOGGING_CATEGORY(LLMComplexItemLog, "LLMComplexItemLog")

const QString LLMComplexItem::name(LLMComplexItem::tr("LLM"));

const char* LLMComplexItem::jsonComplexItemTypeValue = "LLM";

LLMComplexItem::LLMComplexItem(PlanMasterController* masterController, bool flyView, const QString& kmlOrShpFile)
    : ComplexMissionItem(masterController, flyView)
{
    Q_UNUSED(kmlOrShpFile);

    _editorQml = QStringLiteral("qrc:/maglab/LLMItemEditor.qml");

    _initStrategies();
    _voiceStrategy->setShell(this);
    connect(_voiceStrategy, &VoicePlanStrategy::statusChanged, this, &LLMComplexItem::voiceStatusChanged);
    connect(_voiceStrategy, &VoicePlanStrategy::transcriptReady, this, &LLMComplexItem::voiceTranscriptReady);
    connect(_voiceStrategy, &VoicePlanStrategy::reviewChanged, this, &LLMComplexItem::safetyReviewChanged);

    // Default to Spiral mode
    _planMode = Spiral;
    _activeStrategy = _spiralStrategy;
    _connectStrategy(_activeStrategy);

    // Debounced polygon resize: drag vertex → pathChanged fires → restart timer.
    // 150ms after last drag event, apply resize.
    _polygonResizeTimer.setSingleShot(true);
    _polygonResizeTimer.setInterval(150);
    connect(&_polygonResizeTimer, &QTimer::timeout, this, &LLMComplexItem::_onPolygonResized);
    connect(&_surveyAreaPolygon, &QGCMapPolygon::pathChanged, this, [this]() {
        if (!_polygonChanging) _polygonResizeTimer.start();
    });
}

void LLMComplexItem::_initStrategies()
{
    _voiceStrategy  = new VoicePlanStrategy(this);
    _spiralStrategy = new SpiralPlanStrategy(this);
    _fomStrategy    = new FomPlanStrategy(this);
    _customStrategy = new CustomPlanStrategy(this);
    _squareStrategy = new SquarePlanStrategy(this);
}

void LLMComplexItem::_connectStrategy(LLMPlanStrategy* strategy)
{
    if (strategy == nullptr) {
        return;
    }
    connect(strategy, &LLMPlanStrategy::geometryChanged,         this, &LLMComplexItem::_onStrategyGeometryChanged);
    connect(strategy, &LLMPlanStrategy::complexDistanceChanged,  this, &LLMComplexItem::_onStrategyDistanceChanged);
    connect(strategy, &LLMPlanStrategy::lastSequenceNumberChanged, this, &LLMComplexItem::_onStrategyLastSequenceChanged);

    // Connect each Fact's valueChanged to _setDirty + _rebuildPath
    QList<Fact*> factList = strategy->facts();
    for (Fact* f : factList) {
        connect(f, &Fact::valueChanged, this, &LLMComplexItem::_setDirty);
        connect(f, &Fact::valueChanged, this, &LLMComplexItem::_rebuildPath);
    }
}

void LLMComplexItem::_disconnectStrategy(LLMPlanStrategy* strategy)
{
    if (strategy == nullptr) {
        return;
    }
    disconnect(strategy, nullptr, this, nullptr);

    QList<Fact*> factList = strategy->facts();
    for (Fact* f : factList) {
        disconnect(f, nullptr, this, nullptr);
    }
}

void LLMComplexItem::setPlanMode(int mode)
{
    if (mode < Voice || mode > Square) {
        qCWarning(LLMComplexItemLog) << "Invalid planMode:" << mode;
        return;
    }
    if (mode == _planMode) {
        return;
    }

    _disconnectStrategy(_activeStrategy);

    _planMode = mode;
    _activateStrategy(mode);
    _connectStrategy(_activeStrategy);

    if (_coordinate.isValid()) {
        _activeStrategy->recomputeGeometry(_coordinate);
    }
    // Refresh the map trajectory, area box and counts for the new mode (geometry
    // recompute alone doesn't emit pathChanged on a mode switch).
    _updateSurveyAreaPolygon();

    _setDirty();
    emit planModeChanged(_planMode);
    emit pathChanged();
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged(lastSequenceNumber());
}

void LLMComplexItem::_activateStrategy(int mode)
{
    switch (mode) {
    case Voice:   _activeStrategy = _voiceStrategy;  break;
    case Spiral:  _activeStrategy = _spiralStrategy; break;
    case Fom:     _activeStrategy = _fomStrategy;    break;
    case Custom:  _activeStrategy = _customStrategy; break;
    case Square:  _activeStrategy = _squareStrategy; break;
    default:      _activeStrategy = _spiralStrategy; break;
    }
}

Fact* LLMComplexItem::strategyFact(const QString& factName)
{
    return strategyFactForMode(_planMode, factName);
}

Fact* LLMComplexItem::strategyFactForMode(int mode, const QString& factName)
{
    LLMPlanStrategy* strategy = nullptr;
    switch (mode) {
    case Voice:  strategy = _voiceStrategy;  break;
    case Spiral: strategy = _spiralStrategy; break;
    case Fom:    strategy = _fomStrategy;    break;
    case Custom: strategy = _customStrategy; break;
    case Square: strategy = _squareStrategy; break;
    default:
        qCWarning(LLMComplexItemLog) << "Invalid strategy mode for Fact lookup:" << mode;
        return nullptr;
    }

    if (strategy != nullptr) {
        for (Fact* fact : strategy->facts()) {
            if (fact->name() == factName) {
                return fact;
            }
        }
    }
    return nullptr;
}

bool LLMComplexItem::applyMissionIntent(const QJsonObject& intent, QString& errorString)
{
    // Parse: {"planMode":"spiral"|"fom", "params":{...}}
    if (!intent.value(QStringLiteral("planMode")).isString()) {
        errorString = tr("Missing 'planMode' in intent JSON");
        return false;
    }
    if (!intent.value(QStringLiteral("params")).isObject()) {
        errorString = tr("Missing 'params' in intent JSON");
        return false;
    }

    QString mode = intent[QStringLiteral("planMode")].toString().trimmed().toLower();
    QJsonObject params = intent[QStringLiteral("params")].toObject();

    // "survey" maps to QGC's native Survey complex item rather than an LLM
    // sub-strategy: it is inserted alongside this item and configured here.
    if (mode == QStringLiteral("survey")) {
        return _buildNativeSurvey(params, errorString);
    }

    // Determine target strategy
    LLMPlanStrategy* target = nullptr;
    int targetMode = _planMode;

    if (mode == QStringLiteral("spiral")) {
        target = _spiralStrategy;
        targetMode = Spiral;
    } else if (mode == QStringLiteral("fom")) {
        target = _fomStrategy;
        targetMode = Fom;
    } else if (mode == QStringLiteral("custom")) {
        target = _customStrategy;
        targetMode = Custom;
    } else if (mode == QStringLiteral("square")) {
        target = _squareStrategy;
        targetMode = Square;
    } else {
        errorString = tr("Unknown planMode: '%1'. Use 'spiral', 'fom', 'custom', 'square' or 'survey'.").arg(mode);
        return false;
    }

    // Strict per-mode schema check BEFORE touching the strategy, so a malformed
    // LLM response is rejected cleanly instead of half-applied.
    QString schemaErr;
    auto needPositive = [&](const char* key) -> bool {
        const QString k = QString::fromLatin1(key);
        if (!params.contains(k))      { schemaErr = tr("missing '%1'").arg(k);        return false; }
        if (!params.value(k).isDouble()) { schemaErr = tr("'%1' is not a number").arg(k); return false; }
        if (params.value(k).toDouble() <= 0.0) { schemaErr = tr("'%1' must be > 0").arg(k); return false; }
        return true;
    };
    bool schemaOk = true;
    if (targetMode == Spiral) {
        schemaOk = needPositive("spacing") && needPositive("turns");
    } else if (targetMode == Fom) {
        if (params.contains(QStringLiteral("sideLength"))) {
            schemaOk = needPositive("sideLength");
        } else if (params.contains(QStringLiteral("radius"))) {
            schemaOk = needPositive("radius");
        } else {
            schemaErr = tr("missing 'sideLength' or 'radius'");
            schemaOk = false;
        }
    } else if (targetMode == Square) {
        schemaOk = needPositive("spacing") && needPositive("legs");
    } else if (targetMode == Custom) {
        const QJsonValue offsetsValue = params.value(QStringLiteral("offsets"));
        const QJsonArray offs = offsetsValue.toArray();
        if (!offsetsValue.isArray() || offs.size() < 2 || offs.size() > 500) {
            schemaErr = tr("'offsets' needs 2 to 500 points");
            schemaOk = false;
        } else {
            for (const QJsonValue& value : offs) {
                const QJsonObject point = value.toObject();
                if (!value.isObject() || !point.value(QStringLiteral("e")).isDouble() ||
                        !point.value(QStringLiteral("n")).isDouble() ||
                        qAbs(point.value(QStringLiteral("e")).toDouble()) > 10000.0 ||
                        qAbs(point.value(QStringLiteral("n")).toDouble()) > 10000.0) {
                    schemaErr = tr("each offset needs numeric 'e'/'n' within +/-10000 m");
                    schemaOk = false;
                    break;
                }
            }
        }
    }
    if (!schemaOk) {
        errorString = tr("Invalid params for '%1': %2").arg(mode).arg(schemaErr);
        return false;
    }

    // Load params into the target strategy
    if (!target->loadParams(params)) {
        errorString = tr("Failed to load strategy parameters.");
        return false;
    }

    // Switch to the target strategy
    setPlanMode(targetMode);

    // Recompute geometry
    if (_coordinate.isValid()) {
        target->recomputeGeometry(_coordinate);
    }

    _setDirty();
    emit pathChanged();
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged(lastSequenceNumber());

    return true;
}

bool LLMComplexItem::_buildNativeSurvey(const QJsonObject& params, QString& errorString)
{
    if (_missionController == nullptr) {
        errorString = tr("No mission controller available.");
        return false;
    }

    const double width  = params.value(QStringLiteral("width")).toDouble(0.0);
    const double height = params.value(QStringLiteral("height")).toDouble(0.0);
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0 || height <= 0.0) {
        errorString = tr("survey needs positive 'width' and 'height' (m).");
        return false;
    }
    const double spacing  = params.value(QStringLiteral("spacing")).toDouble(30.0);
    const double angle    = params.value(QStringLiteral("angle")).toDouble(0.0);
    if (!std::isfinite(spacing) || spacing <= 0.0 || !std::isfinite(angle)) {
        errorString = tr("Survey spacing must be positive and angle must be finite.");
        return false;
    }
    // Survey is a native QGC item and must keep its own default altitude.
    // Only an explicit Survey intent is allowed to override that default.
    const bool hasAltitude = params.contains(QStringLiteral("altitude")) &&
                             params.value(QStringLiteral("altitude")).isDouble() &&
                             std::isfinite(params.value(QStringLiteral("altitude")).toDouble());
    const double altitude = hasAltitude
                          ? params.value(QStringLiteral("altitude")).toDouble()
                          : 0.0;

    // Centre the survey on this item's coordinate (the map centre where the LLM
    // item was placed), falling back to planned home.
    QGeoCoordinate center = _coordinate;
    if (!center.isValid()) { center = _missionController->plannedHomePosition(); }
    if (!center.isValid()) {
        errorString = tr("No valid centre coordinate for the survey area.");
        return false;
    }

    // Insert the Survey at THIS item's slot so it replaces the LLM console item in
    // place (between Takeoff and Return/Land), not appended after Return-to-Launch.
    // Appending at the end puts a task behind the mission end action and renumbers
    // the whole mission as soon as this item removes itself (see the deferred
    // removal below), which is what broke adding waypoints next to this item.
    // This mirrors SketchComplexItem::finishSurvey().
    int insertIndex = _missionController->visualItems()
                    ? _missionController->visualItems()->indexOf(this) : -1;
    if (insertIndex < 0) {
        errorString = tr("The LLM item is no longer part of this mission.");
        return false;
    }
    VisualMissionItem* vmi = _missionController->insertComplexMissionItem(
        QString::fromUtf8(SurveyComplexItem::canonicalName), center, insertIndex, true /* makeCurrentItem */);
    SurveyComplexItem* survey = qobject_cast<SurveyComplexItem*>(vmi);
    if (survey == nullptr) {
        errorString = tr("Failed to create native Survey item.");
        return false;
    }

    // Axis-aligned rectangle around the centre: width = E-W, height = N-S.
    const double halfW = width  / 2.0;
    const double halfH = height / 2.0;
    const QGeoCoordinate north = center.atDistanceAndAzimuth(halfH, 0.0);
    const QGeoCoordinate south = center.atDistanceAndAzimuth(halfH, 180.0);
    QList<QGeoCoordinate> corners;
    corners << north.atDistanceAndAzimuth(halfW, -90.0)   // NW
            << north.atDistanceAndAzimuth(halfW,  90.0)   // NE
            << south.atDistanceAndAzimuth(halfW,  90.0)   // SE
            << south.atDistanceAndAzimuth(halfW, -90.0);  // SW

    QGCMapPolygon* poly = survey->surveyAreaPolygon();
    poly->clear();
    poly->appendVertices(corners);

    // Manual grid: set altitude + transect spacing + grid angle directly
    // (mirrors SurveyComplexItemTest's manual-camera configuration).
    CameraCalc* cc = survey->cameraCalc();
    cc->setCameraBrand(CameraCalc::canonicalManualCameraName());
    cc->valueSetIsDistance()->setRawValue(true);
    if (hasAltitude) {
        cc->distanceToSurface()->setRawValue(altitude);
    }
    cc->adjustedFootprintSide()->setRawValue(spacing);
    survey->gridAngle()->setRawValue(angle);

    _setDirty();

    // Coordinate: the native Survey is now the real task, so remove this LLM
    // console item to avoid two tasks. Deferred so we don't delete ourselves
    // while still inside this call (invoked from the voice/LLM reply path).
    MissionController* mc = _missionController;
    QPointer<LLMComplexItem> self(this);
    QTimer::singleShot(0, mc, [mc, self]() {
        if (!self || mc->visualItems() == nullptr) { return; }
        int idx = mc->visualItems()->indexOf(self.data());
        if (idx >= 0) { mc->removeVisualItem(idx); }
    });

    return true;
}

QString LLMComplexItem::voiceStatusText() const
{
    return _voiceStrategy ? _voiceStrategy->statusText() : QString();
}

QString LLMComplexItem::safetyReviewText() const
{
    return _voiceStrategy ? _voiceStrategy->reviewText() : QString();
}

bool LLMComplexItem::safetyReviewBusy() const
{
    return _voiceStrategy ? _voiceStrategy->reviewBusy() : false;
}

void LLMComplexItem::reviewSafety()
{
    if (_voiceStrategy == nullptr) { return; }

    QString modeName;
    switch (_planMode) {
    case Voice:  modeName = QStringLiteral("Voice");  break;
    case Spiral: modeName = QStringLiteral("Spiral"); break;
    case Fom:    modeName = QStringLiteral("FOM");    break;
    case Custom: modeName = QStringLiteral("Custom"); break;
    case Square: modeName = QStringLiteral("Square"); break;
    default:     modeName = QStringLiteral("?");      break;
    }

    QString paramsJson;
    if (_activeStrategy != nullptr) {
        paramsJson = QString::fromUtf8(QJsonDocument(_activeStrategy->saveParams())
                                       .toJson(QJsonDocument::Compact));
    }

    int altMode = (_activeStrategy && _activeStrategy->altitudeMode())
                ? _activeStrategy->altitudeMode()->rawValue().toInt() : 0;
    QString altModeStr = altMode == 1 ? QStringLiteral("AMSL")
                       : altMode == 2 ? QStringLiteral("Terrain (AGL)")
                                      : QStringLiteral("Relative-to-takeoff");

    const double alt  = _activeStrategy ? _activeStrategy->minAMSLAltitude() : 0.0;
    const double dist = complexDistance();
    const double tmin = estimatedFlightTime() / 60.0;
    const double spd  = specifiedFlightSpeed();
    const bool   offline = _controllerVehicle ? _controllerVehicle->isOfflineEditingVehicle() : true;

    QString ctx;
    ctx += QStringLiteral("Pre-flight mission safety review request.\n");
    ctx += QStringLiteral("Plan mode: %1\n").arg(modeName);
    ctx += QStringLiteral("Parameters (JSON): %1\n").arg(paramsJson);
    ctx += QStringLiteral("Altitude: %1 m (%2)\n").arg(alt, 0, 'f', 1).arg(altModeStr);
    if (!std::isnan(spd)) {
        ctx += QStringLiteral("Flight speed: %1 m/s\n").arg(spd, 0, 'f', 1);
    }
    ctx += QStringLiteral("Waypoints: %1, path length: %2 m, estimated time: %3 min\n")
            .arg(waypointCount()).arg(dist, 0, 'f', 0).arg(tmin, 0, 'f', 1);
    ctx += QStringLiteral("Vehicle link: %1\n")
            .arg(offline ? QStringLiteral("offline editing (no live telemetry/battery)")
                         : QStringLiteral("connected"));
    if (_missionController && _missionController->visualItems()) {
        ctx += QStringLiteral("Total mission items: %1\n")
                .arg(_missionController->visualItems()->count());
    }

    _voiceStrategy->reviewSafety(ctx);
    emit safetyReviewChanged();
}

void LLMComplexItem::sendToLLM(const QString& text)
{
    if (_voiceStrategy != nullptr) {
        _voiceStrategy->sendToLLM(text);
    }
}

void LLMComplexItem::startVoiceRecording()
{
    if (_voiceStrategy != nullptr) {
        _voiceStrategy->startRecording();
    }
}

void LLMComplexItem::stopVoiceRecording()
{
    if (_voiceStrategy != nullptr) {
        _voiceStrategy->stopRecordingAndTranscribe();
    }
}

QVariantList LLMComplexItem::path() const
{
    QVariantList result;
    if (_activeStrategy == nullptr) {
        return result;
    }
    QList<QGeoCoordinate> pts = _activeStrategy->path();
    qCDebug(LLMComplexItemLog) << "path() called, points:" << pts.size();
    result.reserve(pts.size());
    for (const QGeoCoordinate& pt : pts) {
        result.append(QVariant::fromValue(pt));
    }
    return result;
}

QVariantList LLMComplexItem::arcBoundaryPath() const
{
    QVariantList result;
    const SpiralPlanStrategy* spiral = qobject_cast<const SpiralPlanStrategy*>(_activeStrategy);
    if (spiral == nullptr) {
        return result;
    }
    const QList<QGeoCoordinate> points = spiral->arcBoundaryPoints();
    result.reserve(points.size());
    for (const QGeoCoordinate& point : points) {
        result.append(QVariant::fromValue(point));
    }
    return result;
}

int LLMComplexItem::waypointCount() const
{
    return _activeStrategy ? _activeStrategy->lastSequenceNumber() : 0;
}

double LLMComplexItem::estimatedFlightTime()
{
    if (_planMode == Spiral && _spiralStrategy != nullptr) {
        return _spiralStrategy->estimatedFlightTime();
    }
    double dist = complexDistance();
    double speed = specifiedFlightSpeed();
    if (!std::isfinite(dist) || !std::isfinite(speed) || speed <= 0.0 || dist <= 0.0) {
        return 0.0;
    }
    return dist / speed;
}

double LLMComplexItem::spiralFirstArcRadius() const
{
    return _spiralStrategy ? _spiralStrategy->firstArcRadius() : 0.0;
}

double LLMComplexItem::spiralSafeEntrySpeed() const
{
    return _spiralStrategy ? _spiralStrategy->safeEntrySpeed() : 0.0;
}

double LLMComplexItem::spiralRequestedEntryBankAngle() const
{
    return _spiralStrategy ? _spiralStrategy->requestedEntryBankAngle() : 0.0;
}

bool LLMComplexItem::spiralEntrySpeedLimited() const
{
    return _spiralStrategy && _spiralStrategy->entrySpeedLimited();
}

double LLMComplexItem::complexDistance() const
{
    return _activeStrategy ? _activeStrategy->complexDistance() : 0.0;
}

double LLMComplexItem::minAMSLAltitude() const
{
    return amslEntryAlt();
}

double LLMComplexItem::maxAMSLAltitude() const
{
    return amslEntryAlt();
}

QGeoCoordinate LLMComplexItem::coordinate() const
{
    return _coordinate;
}

QGeoCoordinate LLMComplexItem::entryCoordinate() const
{
    if (_activeStrategy != nullptr) {
        const QList<QGeoCoordinate> points = _activeStrategy->path();
        if (!points.isEmpty()) {
            return points.first();
        }
    }
    return _coordinate;
}

QGeoCoordinate LLMComplexItem::exitCoordinate() const
{
    if (_activeStrategy != nullptr) {
        const QList<QGeoCoordinate> points = _activeStrategy->path();
        if (!points.isEmpty()) {
            return points.last();
        }
    }
    return _coordinate;
}

bool LLMComplexItem::exitCoordinateSameAsEntry() const
{
    if (_activeStrategy == nullptr) {
        return true;
    }
    const QList<QGeoCoordinate> points = _activeStrategy->path();
    return points.size() < 2 || points.first().distanceTo(points.last()) < 0.1;
}

double LLMComplexItem::editableAlt() const
{
    if (_activeStrategy && _activeStrategy->altitude()) {
        return _activeStrategy->altitude()->rawValue().toDouble();
    }
    return 0.0;
}

double LLMComplexItem::amslEntryAlt() const
{
    double altitude = _activeStrategy ? _activeStrategy->minAMSLAltitude() : 0.0;
    if (!std::isfinite(altitude)) {
        altitude = 0.0;
    }
    const int mode = (_activeStrategy && _activeStrategy->altitudeMode())
                   ? _activeStrategy->altitudeMode()->rawValue().toInt() : 0;
    if (mode == 1) {
        return altitude;
    }
    const QGeoCoordinate home = _missionController->plannedHomePosition();
    const double homeAltitude = home.altitude();
    return home.isValid() && std::isfinite(homeAltitude)
            ? altitude + homeAltitude
            : altitude;
}

double LLMComplexItem::specifiedFlightSpeed()
{
    return _activeStrategy ? _activeStrategy->specifiedFlightSpeed()
                           : std::numeric_limits<double>::quiet_NaN();
}

int LLMComplexItem::lastSequenceNumber() const
{
    if (_activeStrategy == nullptr) {
        return _sequenceNumber;
    }
    int count = _activeStrategy->lastSequenceNumber();
    const double specifiedSpeed = _activeStrategy->specifiedFlightSpeed();
    if (std::isfinite(specifiedSpeed) && specifiedSpeed > 0.0 && _controllerVehicle != nullptr) {
        // appendMissionItems always restores a finite default/fallback speed.
        ++count;
    }
    return count > 0 ? _sequenceNumber + count - 1 : _sequenceNumber;
}

void LLMComplexItem::setDirty(bool dirty)
{
    if (_dirty != dirty) {
        _dirty = dirty;
        emit dirtyChanged(_dirty);
    }
}

void LLMComplexItem::setCoordinate(const QGeoCoordinate& coordinate)
{
    if (_coordinate == coordinate) {
        return;
    }
    _coordinate = coordinate;
    if (_activeStrategy != nullptr) {
        _activeStrategy->recomputeGeometry(_coordinate);
    }
    _updateSurveyAreaPolygon();
    _setDirty();
    emit coordinateChanged(_coordinate);
}

void LLMComplexItem::setSequenceNumber(int sequenceNumber)
{
    if (_sequenceNumber != sequenceNumber) {
        _sequenceNumber = sequenceNumber;
        emit sequenceNumberChanged(_sequenceNumber);
        emit lastSequenceNumberChanged(lastSequenceNumber());
    }
}

void LLMComplexItem::appendMissionItems(QList<MissionItem*>& items, QObject* missionItemParent)
{
    if (_activeStrategy == nullptr) { return; }

    int seqNum = _sequenceNumber;
    double relAlt = _activeStrategy->minAMSLAltitude();

    MAV_FRAME mavFrame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    if (_activeStrategy->altitudeMode() != nullptr) {
        int mode = _activeStrategy->altitudeMode()->rawValue().toInt();
        if      (mode == 1) { mavFrame = MAV_FRAME_GLOBAL; }              // AMSL
        else if (mode == 2) { mavFrame = MAV_FRAME_GLOBAL_TERRAIN_ALT; }  // terrain follow
    }

    // Build waypoints via active strategy. For the spiral this also computes the
    // curvature-limited speed profile and emits mission-local DO_CHANGE_SPEED items.
    QList<MissionItem*> strategyItems = _activeStrategy->missionItems(relAlt, mavFrame, missionItemParent);

    // Never silently drop individual commands: that would invalidate the sequence
    // range advertised to the following mission item. Reject the entire output.
    bool validOutput = strategyItems.size() == _activeStrategy->lastSequenceNumber();
    for (const MissionItem* item : strategyItems) {
        if (!item) {
            validOutput = false;
            break;
        }
        const bool navigationItem = item->command() == MAV_CMD_NAV_WAYPOINT ||
                                    item->command() == MAV_CMD_NAV_SPLINE_WAYPOINT;
        if (navigationItem &&
                (!std::isfinite(item->param5()) || !std::isfinite(item->param6()) ||
                 !std::isfinite(item->param7()) ||
                 !QGeoCoordinate(item->param5(), item->param6()).isValid())) {
            validOutput = false;
            break;
        }
    }
    if (!validOutput) {
        qCWarning(LLMComplexItemLog) << "Rejecting invalid or inconsistently counted strategy output";
        qDeleteAll(strategyItems);
        return;
    }

    // Keep speed/acceleration settings isolated to this mission item. Do NOT write
    // WPNAV_* parameters here: those are persistent vehicle-global parameters, so
    // changing them for Spiral would leak into following Survey/Sketch/Waypoint work.
    // Per-item speed is carried by DO_CHANGE_SPEED generated by the active strategy.
    for (MissionItem* mi : strategyItems) {
        // ArduPilot rejects NaN in navigation command parameters (notably
        // MAV_CMD_NAV_SPLINE_WAYPOINT Param4). Zero means no explicit yaw.
        if (!std::isfinite(mi->param1())) { mi->setParam1(0.0); }
        if (!std::isfinite(mi->param2())) { mi->setParam2(0.0); }
        if (!std::isfinite(mi->param3())) { mi->setParam3(0.0); }
        if (!std::isfinite(mi->param4())) { mi->setParam4(0.0); }

        mi->setSequenceNumber(seqNum++);
        items.append(mi);
    }

    // Restore the vehicle's default mission speed after this complex item. MAV_CMD_DO_CHANGE_SPEED
    // is mission-stateful: without this, Spiral/FOM/Square speeds leak into following Survey/Sketch
    // items. This is intentionally a mission item (not WPNAV_* PARAM_SET), so it is local to the
    // uploaded mission and does not alter vehicle-global parameters.
    const double specifiedSpeed = _activeStrategy->specifiedFlightSpeed();
    if (std::isfinite(specifiedSpeed) && specifiedSpeed > 0.0 && _controllerVehicle != nullptr) {
        double restoreSpeed = (_controllerVehicle->multiRotor() || _controllerVehicle->vtol())
                            ? _controllerVehicle->defaultHoverSpeed()
                            : _controllerVehicle->defaultCruiseSpeed();
        if (!std::isfinite(restoreSpeed) || restoreSpeed <= 0.0) {
            restoreSpeed = 5.0;
        }
        MissionItem* restore = new MissionItem(0, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
                                               1, restoreSpeed, -1, 0, 0, 0, 0,
                                               true, false, missionItemParent);
        restore->setSequenceNumber(seqNum++);
        items.append(restore);
    }

    // Flight path segments
    QGeoCoordinate home = _missionController->plannedHomePosition();
    const int altitudeMode = (_activeStrategy->altitudeMode() != nullptr)
                           ? _activeStrategy->altitudeMode()->rawValue().toInt() : 0;
    const double homeAltitude = home.altitude();
    double amslAlt = (altitudeMode == 1 || !home.isValid() || !std::isfinite(homeAltitude))
                   ? relAlt : relAlt + homeAltitude;
    if (!std::isfinite(amslAlt)) {
        amslAlt = 0.0;
    }
    QList<QGeoCoordinate> pts = _activeStrategy->path();
    for (int i = 1; i < pts.size(); ++i) {
        _appendFlightPathSegment(FlightPathSegment::SegmentTypeGeneric,
                                  pts[i-1], amslAlt, pts[i], amslAlt);
    }
}

void LLMComplexItem::setMissionFlightStatus(MissionFlightStatus_t& missionFlightStatus)
{
    ComplexMissionItem::setMissionFlightStatus(missionFlightStatus);

    // Track vehicle speed for time estimates (Survey pattern)
    if (_activeStrategy != nullptr) {
        double dist = complexDistance();
        double speed = specifiedFlightSpeed();
        if (std::isfinite(dist) && std::isfinite(speed) && speed > 0.0 && dist > 0.0) {
            missionFlightStatus.totalDistance += dist;
            missionFlightStatus.totalTime += dist / speed;
            missionFlightStatus.cruiseDistance += dist;
        }
    }
}

void LLMComplexItem::applyNewAltitude(double newAltitude)
{
    Q_UNUSED(newAltitude);
    // LLM pattern does not support altitude adjustment via this path.
    // Altitude is controlled by sub-mode Facts.
}

bool LLMComplexItem::validateMissionOutput() const
{
    if (readyForSaveState() != ReadyForSave) {
        return false;
    }
    const int declaredCount = _activeStrategy->lastSequenceNumber();
    const int mode = _activeStrategy->altitudeMode()->rawValue().toInt();
    const MAV_FRAME frame = mode == 1 ? MAV_FRAME_GLOBAL :
                            mode == 2 ? MAV_FRAME_GLOBAL_TERRAIN_ALT : MAV_FRAME_GLOBAL_RELATIVE_ALT;
    QObject owner;
    const QList<MissionItem*> generated = _activeStrategy->missionItems(
        _activeStrategy->minAMSLAltitude(), frame, &owner);
    if (generated.isEmpty() || generated.size() != declaredCount ||
            _activeStrategy->lastSequenceNumber() != declaredCount) {
        return false;
    }
    for (const MissionItem* item : generated) {
        if (!item) {
            return false;
        }
        const int command = item->command();
        if (command == MAV_CMD_NAV_WAYPOINT || command == MAV_CMD_NAV_SPLINE_WAYPOINT) {
            if (item->frame() != frame || !std::isfinite(item->param7()) ||
                    !std::isfinite(item->param5()) || !std::isfinite(item->param6()) ||
                    !QGeoCoordinate(item->param5(), item->param6()).isValid()) {
                return false;
            }
        } else if (command == MAV_CMD_DO_CHANGE_SPEED) {
            if (!std::isfinite(item->param2()) || item->param2() <= 0.0) {
                return false;
            }
        } else if (command == MAV_CMD_CONDITION_YAW) {
            if (!std::isfinite(item->param1()) || !std::isfinite(item->param2()) ||
                    !std::isfinite(item->param3()) || !std::isfinite(item->param4())) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

VisualMissionItem::ReadyForSaveState LLMComplexItem::readyForSaveState() const
{
    if (_activeStrategy == nullptr || _activeStrategy->path().size() < 2) {
        return NotReadyForSaveData;
    }
    const double altitude = _activeStrategy->minAMSLAltitude();
    const double speed = _activeStrategy->specifiedFlightSpeed();
    Fact* modeFact = _activeStrategy->altitudeMode();
    const double mode = modeFact ? modeFact->rawValue().toDouble() : -1.0;
    if (!std::isfinite(altitude) || !std::isfinite(speed) || speed <= 0.0 ||
            (mode != 0.0 && mode != 1.0 && mode != 2.0)) {
        return NotReadyForSaveData;
    }
    for (const QGeoCoordinate& point : _activeStrategy->path()) {
        if (!point.isValid() ||
                !std::isfinite(point.latitude()) ||
                !std::isfinite(point.longitude())) {
            return NotReadyForSaveData;
        }
    }
    return ReadyForSave;
}

double LLMComplexItem::greatestDistanceTo(const QGeoCoordinate& other) const
{
    if (_activeStrategy == nullptr) {
        return 0.0;
    }
    QList<QGeoCoordinate> pts = _activeStrategy->path();
    double maxDist = 0.0;
    for (const QGeoCoordinate& pt : pts) {
        double d = pt.distanceTo(other);
        if (std::isfinite(d) && d > maxDist) {
            maxDist = d;
        }
    }
    return maxDist;
}

void LLMComplexItem::save(QJsonArray& missionItems)
{
    QJsonObject obj;

    obj[QStringLiteral("type")]            = VisualMissionItem::jsonTypeComplexItemValue;
    obj[QStringLiteral("complexItemType")] = QString::fromLatin1(jsonComplexItemTypeValue);
    obj[QStringLiteral("version")]         = 1;
    obj[QStringLiteral("planMode")]        = _planMode;

    QJsonObject centerObj;
    centerObj[QStringLiteral("lat")]       = _coordinate.latitude();
    centerObj[QStringLiteral("lon")]       = _coordinate.longitude();
    centerObj[QStringLiteral("alt")]       = std::isfinite(_coordinate.altitude())
                                            ? _coordinate.altitude()
                                            : 0.0;
    obj[QStringLiteral("center")]          = centerObj;

    if (_activeStrategy != nullptr) {
        obj[QStringLiteral("strategyParams")] = _activeStrategy->saveParams();
    }

    missionItems.append(obj);
}

bool LLMComplexItem::load(const QJsonObject& complexObject, int sequenceNumber, QString& errorString)
{
    QList<JsonParsing::KeyValidateInfo> keyInfoList = {
        { "planMode", QJsonValue::Double, true },
        { "center",   QJsonValue::Object, true },
        { "version",  QJsonValue::Double, true },
    };
    if (!JsonParsing::validateKeys(complexObject, keyInfoList, errorString)) {
        return false;
    }

    int version = complexObject[QStringLiteral("version")].toInt();
    if (version != 1) {
        errorString = tr("LLM complex item version %1 not supported").arg(version);
        return false;
    }

    int planMode = complexObject[QStringLiteral("planMode")].toInt();
    if (planMode < Voice || planMode > Square) {
        errorString = tr("Unknown LLM planMode: %1").arg(planMode);
        return false;
    }

    QJsonObject centerObj = complexObject[QStringLiteral("center")].toObject();
    _coordinate = QGeoCoordinate(centerObj[QStringLiteral("lat")].toDouble(),
                                  centerObj[QStringLiteral("lon")].toDouble(),
                                  centerObj[QStringLiteral("alt")].toDouble());

    // Activate the saved planMode (but don't emit unnecessary signals during load)
    _disconnectStrategy(_activeStrategy);
    _planMode = planMode;
    _activateStrategy(planMode);
    _connectStrategy(_activeStrategy);

    // Restore strategy params
    if (complexObject.contains(QStringLiteral("strategyParams"))) {
        QJsonObject stratParams = complexObject[QStringLiteral("strategyParams")].toObject();
        if (!_activeStrategy->loadParams(stratParams)) {
            errorString = tr("Failed to load LLM strategy params");
            return false;
        }
    }

    // Recompute geometry
    if (_coordinate.isValid()) {
        _activeStrategy->recomputeGeometry(_coordinate);
    }

    _sequenceNumber = sequenceNumber;
    _dirty = false;

    return true;
}

void LLMComplexItem::_rebuildPath()
{
    if (_activeStrategy != nullptr && _coordinate.isValid()) {
        _activeStrategy->recomputeGeometry(_coordinate);
    }
    _updateSurveyAreaPolygon();
    emit pathChanged();
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged(lastSequenceNumber());
}

void LLMComplexItem::_updateSurveyAreaPolygon()
{
    if (_polygonChanging || !_coordinate.isValid() || _activeStrategy == nullptr) { return; }

    double outerR = 0.0;
    QList<QGeoCoordinate> pts = _activeStrategy->path();
    for (const QGeoCoordinate& pt : pts) {
        double d = _coordinate.distanceTo(pt);
        if (std::isfinite(d) && d > outerR) { outerR = d; }
    }
    if (outerR < 5.0) { outerR = 20.0; }  // fallback for empty/single-point path

    _polygonChanging = true;
    _surveyAreaPolygon.beginReset();
    _surveyAreaPolygon.clear();
    _surveyAreaPolygon.appendVertex(_coordinate.atDistanceAndAzimuth(outerR, 45.0));
    _surveyAreaPolygon.appendVertex(_coordinate.atDistanceAndAzimuth(outerR, 135.0));
    _surveyAreaPolygon.appendVertex(_coordinate.atDistanceAndAzimuth(outerR, 225.0));
    _surveyAreaPolygon.appendVertex(_coordinate.atDistanceAndAzimuth(outerR, 315.0));
    _surveyAreaPolygon.endReset();
    _polygonChanging = false;
}

void LLMComplexItem::_onPolygonResized()
{
    if (_polygonChanging || _activeStrategy == nullptr) { return; }

    QVariantList verts = _surveyAreaPolygon.path();
    int n = verts.size();
    if (n < 3) { return; }

    // Tell the two gestures apart from the resulting polygon shape:
    //  - dragging ONE corner   → the quad becomes irregular → resize about the
    //    FIXED centre so all four corners snap back symmetric (no centre drift);
    //  - dragging the whole box → stays ~regular → translate, size unchanged.
    double avgLat = 0.0, avgLon = 0.0;
    for (int i = 0; i < n; ++i) {
        QGeoCoordinate v = verts[i].value<QGeoCoordinate>();
        avgLat += v.latitude(); avgLon += v.longitude();
    }
    QGeoCoordinate centroid(avgLat / n, avgLon / n, _coordinate.altitude());

    double maxDc = 0.0, minDc = 1e12, sumDc = 0.0;
    for (int i = 0; i < n; ++i) {
        double d = centroid.distanceTo(verts[i].value<QGeoCoordinate>());
        maxDc = qMax(maxDc, d); minDc = qMin(minDc, d); sumDc += d;
    }
    double avgDc = sumDc / n;
    bool irregular = (avgDc > 1.0) && ((maxDc - minDc) > 0.15 * avgDc);

    QGeoCoordinate newCenter;
    double newR;
    if (irregular) {
        newCenter = _coordinate;                 // corner drag → keep centre fixed
        newR = 0.0;
        for (int i = 0; i < n; ++i) {
            newR = qMax(newR, _coordinate.distanceTo(verts[i].value<QGeoCoordinate>()));
        }
    } else {
        newCenter = centroid;                    // whole-box drag → translate
        newR = avgDc;
    }
    if (newR < 5.0) { newR = 20.0; }

    bool centerMoved = (newCenter.distanceTo(_coordinate) > 0.5);
    SpiralPlanStrategy* spiral = qobject_cast<SpiralPlanStrategy*>(_activeStrategy);
    if (spiral != nullptr) {
        // A pure translate (regular box) must NOT change size. Only a corner drag
        // (irregular) resizes — and then turns is recovered with the CORRECT inverse
        // of outerR = rInner + spacing·turns, i.e. turns = (outerR − rInner)/spacing
        // (the old newR/spacing − 1 assumed rInner == spacing and shrank on every drag).
        if (!irregular && !centerMoved) { return; }

        _coordinate = newCenter;
        _polygonChanging = true;
        if (irregular) {
            double spacing = qMax(1.0, spiral->spacing()->rawValue().toDouble());
            double rInner  = qMax(spiral->startRadius()->rawValue().toDouble(), 3.0);
            double newTurns = qMax(0.5, (newR - rInner) / spacing);
            spiral->turns()->setRawValue(newTurns);
        }
        _activeStrategy->recomputeGeometry(_coordinate);
        _polygonChanging = false;
        _updateSurveyAreaPolygon();
    }

    FomPlanStrategy* fom = qobject_cast<FomPlanStrategy*>(_activeStrategy);
    if (fom != nullptr) {
        // Square: corner distance from centre R = (L/2)·√2 → L = R·√2.
        double newSide = newR * 1.41421356;
        double oldSide = fom->sideLength()->rawValue().toDouble();
        bool sizeChanged = (qAbs(oldSide - newSide) >= 0.5);
        if (!centerMoved && !sizeChanged) { return; }
        _coordinate = newCenter;
        _polygonChanging = true;
        if (sizeChanged) {
            fom->sideLength()->setRawValue(newSide);
        }
        _activeStrategy->recomputeGeometry(_coordinate);
        _polygonChanging = false;
        _updateSurveyAreaPolygon();
    }

    _setDirty();
    emit pathChanged();
    emit coordinateChanged(_coordinate);
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged(lastSequenceNumber());
}

void LLMComplexItem::_setDirty()
{
    setDirty(true);
}

void LLMComplexItem::_onStrategyGeometryChanged()
{
    // Strategies also rebuild directly when their own Facts or the center change.
    // Notify exit-coordinate consumers so the following leg (including RTL) updates.
    emit pathChanged();
    emit exitCoordinateChanged(exitCoordinate());
    emit exitCoordinateSameAsEntryChanged(exitCoordinateSameAsEntry());
    emit complexDistanceChanged();
    emit lastSequenceNumberChanged(lastSequenceNumber());
}

void LLMComplexItem::_onStrategyDistanceChanged()
{
    emit complexDistanceChanged();
}

void LLMComplexItem::_onStrategyLastSequenceChanged()
{
    emit lastSequenceNumberChanged(lastSequenceNumber());
}
