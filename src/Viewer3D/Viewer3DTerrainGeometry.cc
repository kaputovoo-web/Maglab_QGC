#include "Viewer3DTerrainGeometry.h"

#include "Fact.h"
#include "QGCGeo.h"
#include "QGCLoggingCategory.h"
#include "SettingsManager.h"
#include "TerrainQuery.h"
#include "Viewer3DSettings.h"

#include <QtCore/QByteArray>
#include <QtCore/QTimer>

#include <algorithm>
#include <cmath>

QGC_LOGGING_CATEGORY(Viewer3DTerrainGeometryLog, "Viewer3d.Viewer3DTerrainGeometry")

static constexpr double kMaxLatitude = 85.05112878;

Viewer3DTerrainGeometry::Viewer3DTerrainGeometry()
    : _elevationTimeout(new QTimer(this))
{
    _elevationTimeout->setSingleShot(true);
    _elevationTimeout->setInterval(30000);
    connect(_elevationTimeout, &QTimer::timeout, this, [this]() {
        _invalidateElevationData();
        updateEarthData();
        _setElevationState(Failed);
        emit elevationDataReceived(false);
    });
    auto *viewer3DSettings = SettingsManager::instance()->viewer3DSettings();
    connect(viewer3DSettings->osmFilePath(), &Fact::rawValueChanged, this, &Viewer3DTerrainGeometry::_clearScene);
    connect(this, &Viewer3DTerrainGeometry::refCoordinateChanged, this, &Viewer3DTerrainGeometry::updateEarthData);
}

void Viewer3DTerrainGeometry::updateEarthData()
{
    clear();
    const int stride = 3 * sizeof(float)
                     + 3 * sizeof(float)   // normals
                     + 2 * sizeof(float);  // UV

    if (!_buildTerrain(roiMin(), roiMax(), refCoordinate(), true, _elevations, _referenceElevation)) {
        qCDebug(Viewer3DTerrainGeometryLog) << "buildTerrain returned false (sector/stack count likely 0)";
        return;
    }

    QByteArray vertexData;
    vertexData.resize(_vertices.size() * stride);
    float *p = reinterpret_cast<float *>(vertexData.data());

    for (size_t i = 0; i < _vertices.size(); ++i) {
        *p++ = _vertices[i].x();
        *p++ = _vertices[i].y();
        *p++ = _vertices[i].z();

        *p++ = _normals[i].x();
        *p++ = _normals[i].y();
        *p++ = _normals[i].z();

        *p++ = _texCoords[i].x();
        *p++ = _texCoords[i].y();
    }

    qCDebug(Viewer3DTerrainGeometryLog) << "Terrain built:" << _vertices.size() << "vertices," << _sectorCount << "sectors," << _stackCount << "stacks";

    setVertexData(vertexData);
    setStride(stride);

    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,
                 0,
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic,
                 3 * sizeof(float),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoordSemantic,
                 6 * sizeof(float),
                 QQuick3DGeometry::Attribute::F32Type);

    if (!_vertices.empty()) {
        QVector3D minimum = _vertices.front();
        QVector3D maximum = minimum;
        for (const QVector3D &vertex : _vertices) {
            for (int axis = 0; axis < 3; ++axis) {
                minimum[axis] = std::fmin(minimum[axis], vertex[axis]);
                maximum[axis] = std::fmax(maximum[axis], vertex[axis]);
            }
        }
        setBounds(minimum, maximum);
    }
    update();
}

QVector3D Viewer3DTerrainGeometry::_computeFaceNormal(const QVector3D &x1, const QVector3D &x2, const QVector3D &x3)
{
    constexpr float EPSILON = 0.000001f;

    QVector3D normal(0, 0, 0);

    const float ex1 = x2.x() - x1.x();
    const float ey1 = x2.y() - x1.y();
    const float ez1 = x2.z() - x1.z();
    const float ex2 = x3.x() - x1.x();
    const float ey2 = x3.y() - x1.y();
    const float ez2 = x3.z() - x1.z();

    const float nx = ey1 * ez2 - ez1 * ey2;
    const float ny = ez1 * ex2 - ex1 * ez2;
    const float nz = ex1 * ey2 - ey1 * ex2;

    const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (length > EPSILON) {
        const float lengthInv = 1.0f / length;
        normal.setX(nx * lengthInv);
        normal.setY(ny * lengthInv);
        normal.setZ(nz * lengthInv);
    }

    return normal;
}

double Viewer3DTerrainGeometry::_gridLatitude(double north, double south, int row, int rows)
{
    // Equal Mercator spacing matches the raster texture's vertical axis.
    const double northY = std::asinh(std::tan(qDegreesToRadians(north)));
    const double southY = std::asinh(std::tan(qDegreesToRadians(south)));
    return qRadiansToDegrees(std::atan(std::sinh(northY + (southY - northY) * row / rows)));
}

void Viewer3DTerrainGeometry::_setElevationState(ElevationState state)
{
    if (_elevationState == state) {
        return;
    }
    _elevationState = state;
    emit elevationStateChanged();
}

void Viewer3DTerrainGeometry::_invalidateElevationData()
{
    _elevationTimeout->stop();
    if (_elevationQuery) {
        disconnect(_elevationQuery, nullptr, this, nullptr);
        _elevationQuery->deleteLater();
        _elevationQuery = nullptr;
    }
    _elevations.clear();
    _previewRequest = false;
    _setElevationState(Idle);
}

double Viewer3DTerrainGeometry::elevationAt(const QGeoCoordinate &coordinate) const
{
    if (_elevationState != Ready || !coordinate.isValid() || _elevations.empty()
        || coordinate.latitude() < _roiMin.latitude() || coordinate.latitude() > _roiMax.latitude()
        || coordinate.longitude() < _roiMin.longitude() || coordinate.longitude() > _roiMax.longitude()) {
        return qQNaN();
    }
    const auto mercator = [](double latitude) { return std::asinh(std::tan(qDegreesToRadians(latitude))); };
    const double x = (coordinate.longitude() - _roiMin.longitude()) / (_roiMax.longitude() - _roiMin.longitude()) * _sectorCount;
    const double y = (mercator(_roiMax.latitude()) - mercator(coordinate.latitude()))
                   / (mercator(_roiMax.latitude()) - mercator(_roiMin.latitude())) * _stackCount;
    const int column = std::clamp(static_cast<int>(x), 0, _sectorCount - 1);
    const int row = std::clamp(static_cast<int>(y), 0, _stackCount - 1);
    const double u = std::clamp(x - column, 0.0, 1.0);
    const double v = std::clamp(y - row, 0.0, 1.0);
    const size_t index = static_cast<size_t>(row) * (_sectorCount + 1) + column;
    // Match the mesh diagonal (NE to SW), not a curved bilinear surface.
    const double northWest = _elevations[index];
    const double northEast = _elevations[index + 1];
    const double southWest = _elevations[index + _sectorCount + 1];
    const double southEast = _elevations[index + _sectorCount + 2];
    const double height = u + v <= 1.0
        ? northWest * (1.0 - u - v) + northEast * u + southWest * v
        : northEast * (1.0 - v) + southWest * (1.0 - u) + southEast * (u + v - 1.0);
    return height - _referenceElevation;
}

void Viewer3DTerrainGeometry::clearElevationData()
{
    _invalidateElevationData();
    updateEarthData();
}

void Viewer3DTerrainGeometry::requestTerrainPreview()
{
    // Only a local visual datum: never exported as vehicle/mission AMSL.
    requestElevationData(0, true);
}

void Viewer3DTerrainGeometry::requestElevationData(double referenceElevation, bool preview)
{
    _invalidateElevationData();
    _previewRequest = preview;
    // Bound this prototype to 1089 samples. DEM resolution is independent of
    // imagery resolution; the caller must set the desired grid explicitly.
    if (_sectorCount <= 0 || _sectorCount > 32 || _stackCount <= 0 || _stackCount > 32
        || !_roiMin.isValid() || !_roiMax.isValid() || !_refCoordinate.isValid()
        || _roiMin.latitude() >= _roiMax.latitude() || _roiMin.longitude() >= _roiMax.longitude()
        || _roiMin.latitude() < -kMaxLatitude || _roiMax.latitude() > kMaxLatitude
        || !std::isfinite(referenceElevation)) {
        updateEarthData();
        _setElevationState(Failed);
        emit elevationDataReceived(false);
        return;
    }

    QList<QGeoCoordinate> coordinates;
    for (int row = 0; row <= _stackCount; ++row) {
        const double latitude = _gridLatitude(_roiMax.latitude(), _roiMin.latitude(), row, _stackCount);
        for (int column = 0; column <= _sectorCount; ++column) {
            const double longitude = _roiMin.longitude() + (_roiMax.longitude() - _roiMin.longitude()) * column / _sectorCount;
            coordinates.append(QGeoCoordinate(latitude, longitude));
        }
    }

    const qsizetype expectedCount = coordinates.size();
    auto *query = new TerrainAtCoordinateQuery(true, this);
    _elevationQuery = query;
    connect(query, &TerrainAtCoordinateQuery::terrainDataReceived, this,
            [this, query, expectedCount, referenceElevation](bool success, const QList<double> &heights) {
        if (_elevationQuery != query) {
            return;
        }
        _elevationQuery = nullptr;
        _elevationTimeout->stop();
        bool valid = success && heights.size() == expectedCount;
        for (double height : heights) {
            valid = valid && std::isfinite(height) && height >= -12000.0 && height <= 10000.0;
        }
        if (valid) {
            _elevations.assign(heights.cbegin(), heights.cend());
            // Center sample defines a reproducible local zero for preview only.
            _referenceElevation = _previewRequest ? heights[(expectedCount - 1) / 2] : referenceElevation;
        }
        updateEarthData();
        _setElevationState(valid ? Ready : Failed);
        emit elevationDataReceived(valid);
    });
    updateEarthData();
    _setElevationState(Loading);
    _elevationTimeout->start();
    query->requestData(coordinates);
}

void Viewer3DTerrainGeometry::_clearScene()
{
    _invalidateElevationData();
    clear();
    setSectorCount(0);
    setStackCount(0);
    _vertices.clear();
    _normals.clear();
    _texCoords.clear();
    update();
}

void Viewer3DTerrainGeometry::setSectorCount(int newSectorCount)
{
    if (_sectorCount == newSectorCount) {
        return;
    }
    _invalidateElevationData();
    _sectorCount = newSectorCount;
    emit sectorCountChanged();
}

void Viewer3DTerrainGeometry::setStackCount(int newStackCount)
{
    if (_stackCount == newStackCount) {
        return;
    }
    _invalidateElevationData();
    _stackCount = newStackCount;
    emit stackCountChanged();
}

bool Viewer3DTerrainGeometry::_buildTerrain(const QGeoCoordinate &roiMinCoordinate, const QGeoCoordinate &roiMaxCoordinate, const QGeoCoordinate &refCoordinate, bool scale,
                                            std::span<const double> elevations, double referenceElevation)
{
    // Keep the local mesh bounded even when called outside the DEM query path.
    // Dateline-crossing and polar regions require a different tiling strategy.
    if (_sectorCount <= 0 || _stackCount <= 0 || _sectorCount > 256 || _stackCount > 256
        || !roiMinCoordinate.isValid() || !roiMaxCoordinate.isValid() || !refCoordinate.isValid()
        || roiMinCoordinate.latitude() >= roiMaxCoordinate.latitude()
        || roiMinCoordinate.longitude() >= roiMaxCoordinate.longitude()
        || roiMinCoordinate.latitude() < -kMaxLatitude || roiMaxCoordinate.latitude() > kMaxLatitude) {
        return false;
    }

    // Samples are row-major, north to south and west to east, with one
    // sample at every grid vertex. Both heights use the same vertical datum.
    const size_t sampleCount = (static_cast<size_t>(_sectorCount) + 1) * (static_cast<size_t>(_stackCount) + 1);
    if (!elevations.empty()) {
        if (elevations.size() != sampleCount || !std::isfinite(referenceElevation)) {
            return false;
        }
        for (const double elevation : elevations) {
            if (!std::isfinite(elevation) || !std::isfinite(static_cast<float>(elevation - referenceElevation))) {
                return false;
            }
        }
    }

    const double sectorLength = roiMaxCoordinate.longitude() - roiMinCoordinate.longitude();
    const double stackRef = roiMaxCoordinate.latitude();
    const double sectorRef = roiMinCoordinate.longitude();

    struct Vertex {
        float x, y, z, s, t;
    };
    std::vector<Vertex> tmpVertices;

    float minT = 10;
    float maxT = -10;
    float minS = 10;
    float maxS = -10;

    // Resolution of each polygon changes by the portion
    const double sectorStep = sectorLength / _sectorCount;
    double sectorAngle, stackAngle;

    for (int i = 0; i <= _stackCount; ++i) {
        stackAngle = _gridLatitude(stackRef, roiMinCoordinate.latitude(), i, _stackCount);

        for (int j = 0; j <= _sectorCount; ++j) {
            sectorAngle = sectorRef + j * sectorStep;

            Vertex vertex;
            const QVector3D localPoint = QGCGeo::convertGpsToEnu(QGeoCoordinate(stackAngle, sectorAngle, 0), refCoordinate);
            vertex.x = localPoint.x();
            vertex.y = localPoint.y();
            const size_t sampleIndex = static_cast<size_t>(i) * (static_cast<size_t>(_sectorCount) + 1) + j;
            vertex.z = elevations.empty() ? 0.0f : static_cast<float>(elevations[sampleIndex] - referenceElevation);

            vertex.s = scale ? static_cast<float>(j) / _sectorCount : (sectorAngle + 180.0) / 360.0;
            minS = std::fmin(minS, vertex.s);
            maxS = std::fmax(maxS, vertex.s);

            if (std::abs(stackAngle) < kMaxLatitude) {
                const double sinLatitude = std::sin(qDegreesToRadians(stackAngle));
                vertex.t = 0.5 - std::log((1 + sinLatitude) / (1 - sinLatitude)) / (4 * M_PI);
            } else {
                vertex.t = (stackRef - stackAngle) / 180;
            }
            if (scale) {
                vertex.t = static_cast<float>(i) / _stackCount;
            }
            minT = std::fmin(minT, vertex.t);
            maxT = std::fmax(maxT, vertex.t);

            tmpVertices.push_back(vertex);
        }
    }

    const float scaleT = maxT - minT;
    const float scaleS = maxS - minS;
    _vertices.clear();
    _texCoords.clear();
    _normals.clear();

    Vertex v1, v2, v3, v4;
    int vi1, vi2;
    QVector3D n;

    for (int i = 0; i < _stackCount; ++i) {
        vi1 = i * (_sectorCount + 1);
        vi2 = (i + 1) * (_sectorCount + 1);
        stackAngle = _gridLatitude(stackRef, roiMinCoordinate.latitude(), i, _stackCount);

        for (int j = 0; j < _sectorCount; ++j, ++vi1, ++vi2) {
            v1 = tmpVertices[vi1];
            v2 = tmpVertices[vi2];
            v3 = tmpVertices[vi1 + 1];
            v4 = tmpVertices[vi2 + 1];

            if (scale) {
                v1.s = (v1.s - minS) / scaleS;
                v1.t = (v1.t - minT) / scaleT;
                v2.s = (v2.s - minS) / scaleS;
                v2.t = (v2.t - minT) / scaleT;
                v3.s = (v3.s - minS) / scaleS;
                v3.t = (v3.t - minT) / scaleT;
                v4.s = (v4.s - minS) / scaleS;
                v4.t = (v4.t - minT) / scaleT;
            }

            if (stackAngle < 90) {
                _vertices.push_back(QVector3D(v1.x, v1.y, v1.z));
                _vertices.push_back(QVector3D(v2.x, v2.y, v2.z));
                _vertices.push_back(QVector3D(v3.x, v3.y, v3.z));

                _texCoords.push_back(QVector2D(v1.s, v1.t));
                _texCoords.push_back(QVector2D(v2.s, v2.t));
                _texCoords.push_back(QVector2D(v3.s, v3.t));

                n = _computeFaceNormal(QVector3D(v1.x, v1.y, v1.z),
                                      QVector3D(v2.x, v2.y, v2.z),
                                      QVector3D(v3.x, v3.y, v3.z));

                for (int k = 0; k < 3; ++k) {
                    _normals.push_back(n);
                }
            }

            if (stackAngle > -90) {
                _vertices.push_back(QVector3D(v3.x, v3.y, v3.z));
                _vertices.push_back(QVector3D(v2.x, v2.y, v2.z));
                _vertices.push_back(QVector3D(v4.x, v4.y, v4.z));

                _texCoords.push_back(QVector2D(v3.s, v3.t));
                _texCoords.push_back(QVector2D(v2.s, v2.t));
                _texCoords.push_back(QVector2D(v4.s, v4.t));

                n = _computeFaceNormal(QVector3D(v3.x, v3.y, v3.z),
                                      QVector3D(v2.x, v2.y, v2.z),
                                      QVector3D(v4.x, v4.y, v4.z));

                for (int k = 0; k < 3; ++k) {
                    _normals.push_back(n);
                }
            }
        }
    }

    return true;
}

void Viewer3DTerrainGeometry::setRoiMin(const QGeoCoordinate &newRoiMin)
{
    if (_roiMin == newRoiMin) {
        return;
    }
    _invalidateElevationData();
    _roiMin = newRoiMin;
    emit roiMinChanged();
}

void Viewer3DTerrainGeometry::setRoiMax(const QGeoCoordinate &newRoiMax)
{
    if (_roiMax == newRoiMax) {
        return;
    }
    _invalidateElevationData();
    _roiMax = newRoiMax;
    emit roiMaxChanged();
}

void Viewer3DTerrainGeometry::setRefCoordinate(const QGeoCoordinate &newRefCoordinate)
{
    if (_refCoordinate == newRefCoordinate) {
        return;
    }
    _invalidateElevationData();
    _refCoordinate = newRefCoordinate;
    emit refCoordinateChanged();
}
