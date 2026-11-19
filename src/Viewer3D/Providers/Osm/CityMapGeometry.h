#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>
#include <QtQuick3D/QQuick3DGeometry>

class OsmParser;
class QVariant;
class Viewer3DMapProvider;
class Viewer3DTerrainGeometry;

class CityMapGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT
    Q_MOC_INCLUDE("Viewer3DMapProvider.h")

    friend class CityMapGeometryTest;

    Q_PROPERTY(Viewer3DTerrainGeometry *terrain READ terrain WRITE setTerrain NOTIFY terrainChanged)
    Q_MOC_INCLUDE("Viewer3DTerrainGeometry.h")

    Q_PROPERTY(QString              modelName   READ modelName   WRITE setModelName   NOTIFY modelNameChanged)
    Q_PROPERTY(Viewer3DMapProvider *mapProvider  READ mapProvider WRITE setMapProvider NOTIFY mapProviderChanged)

public:
    CityMapGeometry();
    Viewer3DTerrainGeometry *terrain() const { return _terrain; }
    void setTerrain(Viewer3DTerrainGeometry *terrain);

    QString modelName() const { return _modelName; }
    void setModelName(const QString &modelName);

    QString osmFilePath() const { return _osmFilePath; }

    Viewer3DMapProvider *mapProvider() const { return _mapProvider; }
    void setMapProvider(Viewer3DMapProvider *newMapProvider);

signals:
    void terrainChanged();
    void modelNameChanged();
    void osmFilePathChanged();
    void mapProviderChanged();

private:
    void _setOsmFilePath(const QVariant &value);
    void _updateViewer();
    void _clearViewer();
    bool _loadOsmMap();

    Viewer3DTerrainGeometry *_terrain = nullptr;
    Viewer3DMapProvider *_mapProvider = nullptr;
    OsmParser *_osmParser = nullptr;

    QString _modelName;
    QString _osmFilePath;
    QByteArray _vertexData;
};
