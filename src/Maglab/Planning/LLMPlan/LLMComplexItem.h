/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "ComplexMissionItem.h"
#include "MissionItem.h"
#include "QGCLoggingCategory.h"
#include "QGCMapPolygon.h"

#include <QGeoCoordinate>
#include <QTimer>
#include <QVariantList>

Q_DECLARE_LOGGING_CATEGORY(LLMComplexItemLog)

class LLMPlanStrategy;
class VoicePlanStrategy;
class SpiralPlanStrategy;
class FomPlanStrategy;
class SquarePlanStrategy;

class CustomPlanStrategy;

/// @brief Shell ComplexMissionItem for the LLM pattern.
///
/// Owns four sub-strategies (Voice, Spiral, FOM, Custom) and delegates all
/// ComplexMissionItem/VisualMissionItem interface methods to the active one.
///
/// To add a new sub-mode:
///   1. Subclass LLMPlanStrategy.
///   2. Add enum value to PlanMode.
///   3. Add factory entry in _initStrategies().
///   4. Add QML editor file + QRC entry.
class LLMComplexItem : public ComplexMissionItem
{
    Q_OBJECT

public:
    enum PlanMode {
        Voice   = 0,
        Spiral  = 1,
        Fom     = 2,
        Custom  = 3,
        Square  = 4,
    };
    Q_ENUM(PlanMode)

    LLMComplexItem(PlanMasterController* masterController, bool flyView, const QString& kmlOrShpFile);

    Q_PROPERTY(int           planMode      READ planMode      WRITE setPlanMode  NOTIFY planModeChanged)
    Q_PROPERTY(QVariantList  path          READ path                              NOTIFY pathChanged)
    Q_PROPERTY(QVariantList  arcBoundaryPath READ arcBoundaryPath                 NOTIFY pathChanged)
    Q_PROPERTY(int           waypointCount READ waypointCount                     NOTIFY pathChanged)
    Q_PROPERTY(double        estimatedFlightTime READ estimatedFlightTime         NOTIFY pathChanged)
    Q_PROPERTY(double        spiralFirstArcRadius READ spiralFirstArcRadius         NOTIFY pathChanged)
    Q_PROPERTY(double        spiralSafeEntrySpeed READ spiralSafeEntrySpeed         NOTIFY pathChanged)
    Q_PROPERTY(double        spiralRequestedEntryBankAngle READ spiralRequestedEntryBankAngle NOTIFY pathChanged)
    Q_PROPERTY(bool          spiralEntrySpeedLimited READ spiralEntrySpeedLimited   NOTIFY pathChanged)
    Q_PROPERTY(QGCMapPolygon* surveyAreaPolygon  READ surveyAreaPolygon             CONSTANT)
    Q_PROPERTY(QString        voiceStatusText    READ voiceStatusText               NOTIFY voiceStatusChanged)
    Q_PROPERTY(QString        safetyReviewText   READ safetyReviewText              NOTIFY safetyReviewChanged)
    Q_PROPERTY(bool           safetyReviewBusy   READ safetyReviewBusy             NOTIFY safetyReviewChanged)

    int          planMode()            const { return _planMode; }
    QVariantList path()                const;
    QVariantList arcBoundaryPath()     const;
    int          waypointCount()       const;
    double       estimatedFlightTime();
    double       spiralFirstArcRadius() const;
    double       spiralSafeEntrySpeed() const;
    double       spiralRequestedEntryBankAngle() const;
    bool         spiralEntrySpeedLimited() const;
    QGCMapPolygon* surveyAreaPolygon() { return &_surveyAreaPolygon; }
    QString        voiceStatusText()   const;
    QString        safetyReviewText()  const;
    bool           safetyReviewBusy()  const;
    Q_INVOKABLE void sendToLLM(const QString& text);
    /// AI pre-flight safety review: summarise this mission and ask the LLM to flag risks.
    Q_INVOKABLE void reviewSafety();
    Q_INVOKABLE void startVoiceRecording();
    Q_INVOKABLE void stopVoiceRecording();

    void setPlanMode(int mode);

    /// Look up a strategy's Fact by name for QML binding.
    /// Example: missionItem.strategyFact("startRadius") returns the Spiral startRadius Fact.
    Q_INVOKABLE Fact* strategyFact(const QString& name);

    /// Look up a Fact from one explicit strategy. This prevents hidden QML
    /// editors from binding to a same-named Fact owned by the active strategy.
    Q_INVOKABLE Fact* strategyFactForMode(int mode, const QString& name);

    /// Future voice seam: apply a Mission-Intent JSON to set planMode + strategy params.
    /// TODO: voice/LLM — implement when cloud multimodal LLM integration is ready.
    Q_INVOKABLE bool applyMissionIntent(const QJsonObject& intent, QString& errorString);

    // Overrides from ComplexMissionItem
    QString patternName()    const final { return name; }
    double  complexDistance() const final;
    double  minAMSLAltitude() const final;
    double  maxAMSLAltitude() const final;
    bool    load(const QJsonObject& complexObject, int sequenceNumber, QString& errorString) final;
    double  greatestDistanceTo(const QGeoCoordinate& other) const final;

    // Overrides from VisualMissionItem
    bool                dirty()                   const final { return _dirty; }
    bool                isSimpleItem()            const final { return false; }
    bool                isStandaloneCoordinate()  const final { return false; }
    bool                specifiesCoordinate()     const final { return true; }
    bool                specifiesAltitudeOnly()   const final { return false; }
    QString             commandDescription()      const final { return tr("LLM Plan"); }
    QString             commandName()             const final { return tr("LLM Plan"); }
    QString             abbreviation()            const final { return QStringLiteral("L"); }
    QGeoCoordinate      coordinate()              const final;
    QGeoCoordinate      entryCoordinate()         const final;
    QGeoCoordinate      exitCoordinate()          const final;
    double              editableAlt()             const final;
    double              amslEntryAlt()            const final;
    double              amslExitAlt()             const final { return amslEntryAlt(); }
    int                 sequenceNumber()          const final { return _sequenceNumber; }
    double              specifiedFlightSpeed()    final;
    double              specifiedGimbalYaw()      final { return std::numeric_limits<double>::quiet_NaN(); }
    double              specifiedGimbalPitch()    final { return std::numeric_limits<double>::quiet_NaN(); }
    void                appendMissionItems(QList<MissionItem*>& items, QObject* missionItemParent) final;
    void                setMissionFlightStatus(MissionFlightStatus_t& missionFlightStatus) final;
    void                applyNewAltitude(double newAltitude) final;
    double              additionalTimeDelay()    const final { return 0.0; }
    ReadyForSaveState   readyForSaveState()      const final;
    void                save(QJsonArray& missionItems) final;
    bool                exitCoordinateSameAsEntry() const final;
    void                setDirty(bool dirty) final;
    void                setCoordinate(const QGeoCoordinate& coordinate) final;
    void                setSequenceNumber(int sequenceNumber) final;
    int                 lastSequenceNumber()  const final;
    QString             mapVisualQML()        const final { return QStringLiteral("qrc:/maglab/LLMMapVisual.qml"); }

    static const QString name;

    static const char* jsonComplexItemTypeValue;

signals:
    void planModeChanged(int planMode);
    void pathChanged();
    void voiceStatusChanged();
    void safetyReviewChanged();
    void voiceTranscriptReady(const QString& text);

private slots:
    void _rebuildPath();
    void _setDirty();
    void _onStrategyGeometryChanged();
    void _onStrategyDistanceChanged();
    void _onStrategyLastSequenceChanged();
    void _onPolygonResized();

public:
    /// Preflight actual strategy output before any save or upload side effects.
    bool validateMissionOutput() const;

private:
    /// Insert + configure a native QGC Survey from an LLM/voice intent
    /// ({width,height,spacing,angle,altitude}), centred on this item.
    bool _buildNativeSurvey(const QJsonObject& params, QString& errorString);

    void _initStrategies();
    void _activateStrategy(int mode);
    void _disconnectStrategy(LLMPlanStrategy* strategy);
    void _connectStrategy(LLMPlanStrategy* strategy);

    int _planMode = Voice;
    int _sequenceNumber = 0;
    bool _dirty = false;
    bool _polygonChanging = false;
    QTimer _polygonResizeTimer;

    void _updateSurveyAreaPolygon();

    QGeoCoordinate        _coordinate;
    QGCMapPolygon         _surveyAreaPolygon;
    LLMPlanStrategy*      _activeStrategy = nullptr;
    VoicePlanStrategy*    _voiceStrategy  = nullptr;
    SpiralPlanStrategy*   _spiralStrategy = nullptr;
    FomPlanStrategy*      _fomStrategy    = nullptr;
    CustomPlanStrategy*   _customStrategy = nullptr;
    SquarePlanStrategy*   _squareStrategy = nullptr;
};
