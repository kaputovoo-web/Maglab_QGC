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
#include "Fact.h"
#include "FactMetaData.h"

#include <QGeoCoordinate>
#include <QVariantList>

Q_DECLARE_LOGGING_CATEGORY(SketchComplexItemLog)

/// @brief Free-hand "Sketch" plan item.
///
/// The user draws on the map by dragging (capture lives in SketchMapVisual.qml).
/// Two modes via a drop-down:
///   - SurveyArea : draw a closed region → spawn a native Survey (full adjust),
///   - Waypoints  : draw a path → straight (NAV_WAYPOINT) or curved (spline) route,
///                  with an optional per-waypoint style override.
class SketchComplexItem : public ComplexMissionItem
{
    Q_OBJECT

public:
    enum DrawMode { SurveyArea = 0, Waypoints = 1 };
    Q_ENUM(DrawMode)

    SketchComplexItem(PlanMasterController* masterController, bool flyView, const QString& kmlFile);

    Q_PROPERTY(int          drawMode    READ drawMode    WRITE setDrawMode   NOTIFY drawModeChanged)
    Q_PROPERTY(int          routeStyle  READ routeStyle  WRITE setRouteStyle NOTIFY routeStyleChanged) // 0=straight 1=curve
    Q_PROPERTY(bool         drawing     READ drawing     WRITE setDrawing    NOTIFY drawingChanged)
    Q_PROPERTY(bool         drawArmed   READ drawArmed   WRITE setDrawArmed  NOTIFY drawArmedChanged)
    Q_PROPERTY(int          pointCount  READ pointCount                      NOTIFY pathChanged)
    Q_PROPERTY(int          selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantList path        READ path                            NOTIFY pathChanged)
    Q_PROPERTY(Fact*        altitude     READ altitude     CONSTANT)
    Q_PROPERTY(Fact*        altitudeMode READ altitudeMode CONSTANT)
    Q_PROPERTY(Fact*        flightSpeed  READ flightSpeed  CONSTANT)
    Q_PROPERTY(Fact*        gridSpacing  READ gridSpacing  CONSTANT)

    int          drawMode()   const { return _drawMode; }
    int          routeStyle() const { return _routeStyle; }
    bool         drawing()    const { return _drawing; }
    bool         drawArmed()  const { return _drawArmed; }
    void         setDrawArmed(bool armed);
    int          pointCount() const { return _points.size(); }
    int          selectedIndex() const { return _selectedIndex; }
    void         setSelectedIndex(int i);
    QVariantList path()       const;
    Fact*        altitude()     { return &_altitude; }
    Fact*        altitudeMode() { return &_altitudeMode; }
    Fact*        flightSpeed()  { return &_flightSpeed; }
    Fact*        gridSpacing()  { return &_gridSpacing; }

    void setDrawMode(int mode);
    void setRouteStyle(int style);
    void setDrawing(bool drawing);

    /// Drawing API, called from SketchMapVisual.qml while the user drags.
    Q_INVOKABLE void beginStroke();
    Q_INVOKABLE void addPoint(const QGeoCoordinate& c);
    Q_INVOKABLE void endStroke();
    Q_INVOKABLE void clearPoints();
    /// Per-waypoint style override (-1 = use global routeStyle, 0 = straight, 1 = curve).
    Q_INVOKABLE void setPointStyle(int index, int style);
    Q_INVOKABLE int  pointStyle(int index) const;
    /// Move an existing waypoint (drag on the map).
    Q_INVOKABLE void movePoint(int index, const QGeoCoordinate& c);
    Q_INVOKABLE void removePoint(int index);
    /// Replace the drawn region with its minimum-area 4-corner quadrilateral.
    Q_INVOKABLE void makeQuad();
    /// In SurveyArea mode: build a native Survey from the drawn polygon, then remove self.
    Q_INVOKABLE void generateSurvey();
    bool finishSurvey(QString& errorString);

    // Overrides from ComplexMissionItem
    QString patternName()     const final { return name; }
    double  complexDistance()  const final { return _complexDistance; }
    double  minAMSLAltitude()  const final { return _altitude.rawValue().toDouble(); }
    double  maxAMSLAltitude()  const final { return _altitude.rawValue().toDouble(); }
    bool    load(const QJsonObject& complexObject, int sequenceNumber, QString& errorString) final;
    double  greatestDistanceTo(const QGeoCoordinate& other) const final;

    // Overrides from VisualMissionItem
    bool                dirty()                   const final { return _dirty; }
    bool                isSimpleItem()            const final { return false; }
    bool                isStandaloneCoordinate()  const final { return false; }
    // A placed-but-not-yet-drawn Sketch still owns a coordinate (its placement
    // anchor), otherwise it stops being a coordinate parent and commands added
    // under it attach to the previous item instead.
    bool                specifiesCoordinate()     const final { return _points.size() > 0 || _coordinate.isValid(); }
    bool                specifiesAltitudeOnly()   const final { return false; }
    QString             commandDescription()      const final { return tr("Sketch"); }
    QString             commandName()             const final { return tr("Sketch"); }
    QString             abbreviation()            const final { return QStringLiteral("Sk"); }
    QGeoCoordinate      coordinate()              const final;
    QGeoCoordinate      entryCoordinate()         const final;
    QGeoCoordinate      exitCoordinate()          const final;
    double              editableAlt()             const final { return _altitude.rawValue().toDouble(); }
    double              amslEntryAlt()            const final { return _altitude.rawValue().toDouble(); }
    double              amslExitAlt()             const final { return _altitude.rawValue().toDouble(); }
    int                 sequenceNumber()          const final { return _sequenceNumber; }
    double              specifiedFlightSpeed()    final { return _flightSpeed.rawValue().toDouble(); }
    double              specifiedGimbalYaw()      final { return std::numeric_limits<double>::quiet_NaN(); }
    double              specifiedGimbalPitch()    final { return std::numeric_limits<double>::quiet_NaN(); }
    void                appendMissionItems(QList<MissionItem*>& items, QObject* missionItemParent) final;
    void                setMissionFlightStatus(MissionFlightStatus_t& missionFlightStatus) final;
    void                applyNewAltitude(double newAltitude) final;
    double              additionalTimeDelay()    const final { return 0.0; }
    ReadyForSaveState   readyForSaveState()      const final;
    void                save(QJsonArray& missionItems) final;
    bool                exitCoordinateSameAsEntry() const final { return false; }
    void                setDirty(bool dirty) final;
    void                setCoordinate(const QGeoCoordinate& coordinate) final;
    void                setSequenceNumber(int sequenceNumber) final;
    int                 lastSequenceNumber()  const final;
    QString             mapVisualQML()        const final { return QStringLiteral("qrc:/maglab/SketchMapVisual.qml"); }

    static const QString name;
    static const char* jsonComplexItemTypeValue;

signals:
    void drawModeChanged(int drawMode);
    void routeStyleChanged(int routeStyle);
    void drawingChanged(bool drawing);
    void drawArmedChanged(bool drawArmed);
    void selectedIndexChanged(int selectedIndex);
    void pathChanged();

private:
    void   _rebuild();
    int    _effectiveStyle(int index) const;
    static QList<QGeoCoordinate> _simplify(const QList<QGeoCoordinate>& pts, double epsM);

    int  _drawMode      = SurveyArea;
    int  _routeStyle    = 1;          // default curve
    bool _drawing       = false;
    bool _drawArmed     = false;
    int  _selectedIndex = -1;
    int  _sequenceNumber = 0;
    bool _dirty         = false;
    double _complexDistance = 0.0;

    QGeoCoordinate        _coordinate;  // placement anchor while _points is empty
    QList<QGeoCoordinate> _points;    // simplified drawn points
    QList<int>            _ptStyle;   // per-point style override (-1/0/1)

    Fact _altitude;
    Fact _altitudeMode;
    Fact _flightSpeed;
    Fact _gridSpacing;
    FactMetaData* _altitudeMeta     = nullptr;
    FactMetaData* _altitudeModeMeta = nullptr;
    FactMetaData* _flightSpeedMeta  = nullptr;
    FactMetaData* _gridSpacingMeta  = nullptr;
};
