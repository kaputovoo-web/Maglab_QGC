#pragma once

#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtPositioning/QGeoCoordinate>
#include <QtQmlIntegration/QtQmlIntegration>
#include <QtQuick3D/QQuick3DGeometry>

#include <span>
#include <vector>

class TerrainAtCoordinateQuery;
class QTimer;

class Viewer3DTerrainGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int            sectorCount   READ sectorCount   WRITE setSectorCount   NOTIFY sectorCountChanged)
    Q_PROPERTY(int            stackCount    READ stackCount    WRITE setStackCount    NOTIFY stackCountChanged)
    Q_PROPERTY(QGeoCoordinate roiMin        READ roiMin        WRITE setRoiMin        NOTIFY roiMinChanged)
    Q_PROPERTY(QGeoCoordinate roiMax        READ roiMax        WRITE setRoiMax        NOTIFY roiMaxChanged)
    Q_PROPERTY(QGeoCoordinate refCoordinate READ refCoordinate WRITE setRefCoordinate NOTIFY refCoordinateChanged)

    Q_PROPERTY(ElevationState elevationState READ elevationState NOTIFY elevationStateChanged)

    friend class Viewer3DTerrainGeometryTest;

public:
    enum ElevationState { Idle, Loading, Ready, Failed };
    Q_ENUM(ElevationState)
    ElevationState elevationState() const { return _elevationState; }

    explicit Viewer3DTerrainGeometry();

    Q_INVOKABLE void updateEarthData();
    Q_INVOKABLE void requestTerrainPreview();
    Q_INVOKABLE void clearElevationData();

    // Explicit opt-in: caller must supply a reference in the DEM vertical datum.
    // Existing QML does not invoke this until vehicle height alignment is ready.
    void requestElevationData(double referenceElevation, bool preview = false);
    // NaN outside loaded coverage; never extend edge heights into unknown areas.
    Q_INVOKABLE double elevationAt(const QGeoCoordinate &coordinate) const;

    int sectorCount() const { return _sectorCount; }
    void setSectorCount(int newSectorCount);

    int stackCount() const { return _stackCount; }
    void setStackCount(int newStackCount);

    QGeoCoordinate roiMin() const { return _roiMin; }
    void setRoiMin(const QGeoCoordinate &newRoiMin);

    QGeoCoordinate roiMax() const { return _roiMax; }
    void setRoiMax(const QGeoCoordinate &newRoiMax);

    QGeoCoordinate refCoordinate() const { return _refCoordinate; }
    void setRefCoordinate(const QGeoCoordinate &newRefCoordinate);

signals:
    void sectorCountChanged();
    void stackCountChanged();
    void roiMinChanged();
    void roiMaxChanged();
    void refCoordinateChanged();
    void elevationDataReceived(bool success);
    void elevationStateChanged();

private:
    bool _buildTerrain(const QGeoCoordinate &roiMinCoordinate, const QGeoCoordinate &roiMaxCoordinate, const QGeoCoordinate &refCoordinate, bool scale,
                       std::span<const double> elevations = {}, double referenceElevation = 0.0);
    static QVector3D _computeFaceNormal(const QVector3D &x1, const QVector3D &x2, const QVector3D &x3);
    static double _gridLatitude(double north, double south, int row, int rows);
    void _clearScene();
    void _invalidateElevationData();
    void _setElevationState(ElevationState state);

    std::vector<QVector3D> _vertices;
    std::vector<QVector2D> _texCoords;
    std::vector<QVector3D> _normals;

    QGeoCoordinate _roiMin;
    QGeoCoordinate _roiMax;
    QGeoCoordinate _refCoordinate;

    QTimer *_elevationTimeout = nullptr;
    TerrainAtCoordinateQuery *_elevationQuery = nullptr;
    std::vector<double> _elevations;
    double _referenceElevation = 0.0;
    bool _previewRequest = false;
    ElevationState _elevationState = Idle;

    int _sectorCount = 0;
    int _stackCount = 0;
};
