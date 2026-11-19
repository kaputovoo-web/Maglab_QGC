#include "CityMapGeometry.h"

#include "Fact.h"
#include "OsmParser.h"
#include "QGCLoggingCategory.h"
#include "QGCGeo.h"
#include "Viewer3DTerrainGeometry.h"
#include "SettingsManager.h"
#include "Viewer3DMapProvider.h"
#include "Viewer3DSettings.h"

QGC_LOGGING_CATEGORY(CityMapGeometryLog, "Viewer3d.CityMapGeometry")

CityMapGeometry::CityMapGeometry()
    : _modelName(QStringLiteral("city_map_default_name"))
{
    Viewer3DSettings* viewer3DSettings = SettingsManager::instance()->viewer3DSettings();

    _setOsmFilePath(viewer3DSettings->osmFilePath()->rawValue());
    connect(viewer3DSettings->osmFilePath(), &Fact::rawValueChanged, this, &CityMapGeometry::_setOsmFilePath);
}

void CityMapGeometry::setTerrain(Viewer3DTerrainGeometry *terrain)
{
    if (_terrain == terrain) {
        return;
    }
    if (_terrain) {
        disconnect(_terrain, nullptr, this, nullptr);
    }
    _terrain = terrain;
    if (_terrain) {
        connect(_terrain, &Viewer3DTerrainGeometry::elevationStateChanged, this, &CityMapGeometry::_updateViewer);
        connect(_terrain, &QObject::destroyed, this, [this]() {
            _terrain = nullptr;
            emit terrainChanged();
            _updateViewer();
        });
    }
    emit terrainChanged();
    _updateViewer();
}

void CityMapGeometry::setModelName(const QString &modelName)
{
    if (_modelName == modelName) {
        return;
    }
    _modelName = modelName;
    emit modelNameChanged();
}

void CityMapGeometry::_setOsmFilePath(const QVariant &value)
{
    _clearViewer();
    _osmFilePath = value.toString();
    qCDebug(CityMapGeometryLog) << "OSM file path set to:" << _osmFilePath;
    emit osmFilePathChanged();
    _loadOsmMap();
}

void CityMapGeometry::setMapProvider(Viewer3DMapProvider *newMapProvider)
{
    if (_mapProvider == newMapProvider) {
        return;
    }

    if (_osmParser) {
        disconnect(_osmParser, &OsmParser::buildingLevelHeightChanged, this, &CityMapGeometry::_updateViewer);
    }
    if (_mapProvider) {
        disconnect(_mapProvider, &Viewer3DMapProvider::mapChanged, this, &CityMapGeometry::_updateViewer);
    }

    _mapProvider = newMapProvider;
    _osmParser = qobject_cast<OsmParser*>(_mapProvider);

    if (_osmParser) {
        connect(_osmParser, &OsmParser::buildingLevelHeightChanged, this, &CityMapGeometry::_updateViewer);
    }
    if (_mapProvider) {
        connect(_mapProvider, &Viewer3DMapProvider::mapChanged, this, &CityMapGeometry::_updateViewer);
    }

    emit mapProviderChanged();
    _loadOsmMap();
}

bool CityMapGeometry::_loadOsmMap()
{
    if (!_osmParser) {
        return false;
    }

    _osmParser->parseOsmFile(_osmFilePath);
    return true;
}

void CityMapGeometry::_updateViewer()
{
    clear();

    if (!_osmParser) {
        qCDebug(CityMapGeometryLog) << "updateViewer: no OSM parser set";
        return;
    }

    if (_osmParser->mapLoaded()) {
        if (_terrain) {
            // Terrain-enabled scenes must not display zero-height foundations
            // while DEM is unavailable. Omit buildings until samples are ready.
            if (_terrain->elevationState() != Viewer3DTerrainGeometry::Ready) {
                update();
                return;
            }
            _vertexData = _osmParser->buildingToMesh([this](const QVector2D &point) {
                const QGeoCoordinate coordinate = QGCGeo::convertEnuToGps(QVector3D(point.x(), point.y(), 0), _osmParser->gpsRef());
                return _terrain->elevationAt(coordinate);
            });
        } else {
            _vertexData = _osmParser->buildingToMesh();
        }
        qCDebug(CityMapGeometryLog) << "Building mesh generated:" << _vertexData.size() << "bytes";

        constexpr int stride = 3 * sizeof(float);
        if (!_vertexData.isEmpty()) {
            setVertexData(_vertexData);
            setStride(stride);
            setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
            addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,
                         0,
                         QQuick3DGeometry::Attribute::F32Type);
        }
        update();
    }
}

void CityMapGeometry::_clearViewer()
{
    clear();
    _vertexData.clear();
    update();
}
