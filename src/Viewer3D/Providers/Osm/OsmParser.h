#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtQmlIntegration/QtQmlIntegration>

#include <functional>
#include <vector>

#include "Viewer3DMapProvider.h"

class OsmParserThread;
class QVariant;

class OsmParser : public Viewer3DMapProvider
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    friend class OsmParserTest;

public:
    explicit OsmParser(QObject *parent = nullptr);
    ~OsmParser() override;

    bool mapLoaded() const override { return _mapLoadedFlag; }
    QGeoCoordinate gpsRef() const override { return _gpsRefPoint; }
    std::pair<QGeoCoordinate, QGeoCoordinate> mapBoundingBox() const override { return {_coordinateMin, _coordinateMax}; }

    float buildingLevelHeight() const { return _buildingLevelHeight; }

    void setGpsRef(const QGeoCoordinate &gpsRef);
    void resetGpsRef();
    void parseOsmFile(const QString &filePath);
    // Optional ground sampler uses local ENU meters; unknown buildings are omitted.
    QByteArray buildingToMesh(const std::function<double(const QVector2D &)> &groundSampler = {});

signals:
    void buildingLevelHeightChanged();

private:
    void _setBuildingLevelHeight(const QVariant &value);
    void _onOsmParserFinished(bool isValid);
    void _triangulateWallsExtrudedPolygon(std::vector<QVector3D> &triangulatedMesh, const std::vector<QVector2D> &verticesCcw, float h, bool inverseOrder);
    void _triangulateRectangle(std::vector<QVector3D> &triangulatedMesh, const std::vector<QVector3D> &verticesCcw, bool invertNormal);

    OsmParserThread *_osmParserWorker = nullptr;

    QGeoCoordinate _gpsRefPoint;
    QGeoCoordinate _coordinateMin;
    QGeoCoordinate _coordinateMax;

    float _buildingLevelHeight = 0;
    bool _gpsRefSet = false;
    bool _mapLoadedFlag = false;
};
