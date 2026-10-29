#include "Viewer3DTerrainGeometryTest.h"
#include <QtTest/QSignalSpy>
#include <QtCore/QTimer>


#include "Viewer3DTerrainGeometry.h"
#include "TerrainQuery.h"

#include <array>
#include <cmath>
#include <limits>

void Viewer3DTerrainGeometryTest::_testComputeFaceNormal()
{
    Viewer3DTerrainGeometry geo;

    QVector3D x1(0, 0, 0);
    QVector3D x2(1, 0, 0);
    QVector3D x3(0, 1, 0);

    QVector3D normal = geo._computeFaceNormal(x1, x2, x3);

    QCOMPARE_FUZZY(normal.x(), 0.0f, 0.0001f);
    QCOMPARE_FUZZY(normal.y(), 0.0f, 0.0001f);
    QCOMPARE_FUZZY(normal.z(), 1.0f, 0.0001f);
}

void Viewer3DTerrainGeometryTest::_testComputeFaceNormalDegenerate()
{
    Viewer3DTerrainGeometry geo;

    // Collinear points — degenerate triangle
    QVector3D x1(0, 0, 0);
    QVector3D x2(1, 0, 0);
    QVector3D x3(2, 0, 0);

    QVector3D normal = geo._computeFaceNormal(x1, x2, x3);

    QCOMPARE_FUZZY(normal.x(), 0.0f, 0.0001f);
    QCOMPARE_FUZZY(normal.y(), 0.0f, 0.0001f);
    QCOMPARE_FUZZY(normal.z(), 0.0f, 0.0001f);
}

void Viewer3DTerrainGeometryTest::_testBuildTerrainZeroCounts()
{
    Viewer3DTerrainGeometry geo;

    geo.setSectorCount(0);
    geo.setStackCount(0);

    QGeoCoordinate roiMin(47.0, 8.0, 0);
    QGeoCoordinate roiMax(47.1, 8.1, 0);
    QGeoCoordinate ref(47.05, 8.05, 0);

    bool result = geo._buildTerrain(roiMin, roiMax, ref, true);
    QVERIFY(!result);
}

void Viewer3DTerrainGeometryTest::_testInvalidTerrainRegion_data()
{
    QTest::addColumn<QGeoCoordinate>("minimum");
    QTest::addColumn<QGeoCoordinate>("maximum");
    QTest::addColumn<int>("sectors");
    QTest::newRow("zero-width") << QGeoCoordinate(35, 120) << QGeoCoordinate(36, 120) << 2;
    QTest::newRow("zero-height") << QGeoCoordinate(35, 120) << QGeoCoordinate(35, 121) << 2;
    QTest::newRow("invalid") << QGeoCoordinate() << QGeoCoordinate(36, 121) << 2;
    QTest::newRow("dateline") << QGeoCoordinate(35, 179) << QGeoCoordinate(36, -179) << 2;
    QTest::newRow("polar") << QGeoCoordinate(85, 120) << QGeoCoordinate(86, 121) << 2;
    QTest::newRow("over-budget") << QGeoCoordinate(35, 120) << QGeoCoordinate(36, 121) << 257;
}

void Viewer3DTerrainGeometryTest::_testInvalidTerrainRegion()
{
    QFETCH(QGeoCoordinate, minimum);
    QFETCH(QGeoCoordinate, maximum);
    QFETCH(int, sectors);
    Viewer3DTerrainGeometry geo;
    geo.setSectorCount(sectors);
    geo.setStackCount(2);
    QVERIFY(!geo._buildTerrain(minimum, maximum, QGeoCoordinate(35.5, 120.5), true));
    QVERIFY(geo._vertices.empty());
}

void Viewer3DTerrainGeometryTest::_testBuildTerrainValidRegion()
{
    Viewer3DTerrainGeometry geo;

    geo.setSectorCount(2);
    geo.setStackCount(2);

    QGeoCoordinate roiMin(47.0, 8.0, 0);
    QGeoCoordinate roiMax(47.01, 8.01, 0);
    QGeoCoordinate ref(47.005, 8.005, 0);

    bool result = geo._buildTerrain(roiMin, roiMax, ref, true);
    QVERIFY(result);

    QVERIFY(!geo._vertices.empty());
    QVERIFY(!geo._normals.empty());
    QVERIFY(!geo._texCoords.empty());
    QCOMPARE(geo._vertices.size(), geo._normals.size());
    QCOMPARE(geo._vertices.size(), geo._texCoords.size());
}

void Viewer3DTerrainGeometryTest::_testElevationQueryInvalidation()
{
    Viewer3DTerrainGeometry geo;
    QSignalSpy resultSpy(&geo, &Viewer3DTerrainGeometry::elevationDataReceived);
    geo.requestElevationData(100);
    QCOMPARE(geo.elevationState(), Viewer3DTerrainGeometry::Failed);
    QCOMPARE(resultSpy.count(), 1);
    QCOMPARE(resultSpy.at(0).at(0).toBool(), false);

    geo.setSectorCount(1);
    geo.setStackCount(1);
    geo.setRoiMin(QGeoCoordinate(35.9, 120.0));
    geo.setRoiMax(QGeoCoordinate(35.91, 120.01));
    geo.setRefCoordinate(QGeoCoordinate(35.905, 120.005));
    geo.requestElevationData(100);
    auto *previousQuery = geo._elevationQuery;
    QVERIFY(previousQuery);
    QCOMPARE(geo.elevationState(), Viewer3DTerrainGeometry::Loading);
    geo.setRoiMin(QGeoCoordinate(35.89, 120.0));
    QCOMPARE(geo.elevationState(), Viewer3DTerrainGeometry::Idle);
    previousQuery->signalTerrainData(true, {110, 120, 130, 140});
    QVERIFY(geo._elevations.empty());
    QCOMPARE(resultSpy.count(), 1);

    geo.requestElevationData(100);
    QVERIFY(geo._elevationQuery);
    geo._elevationQuery->signalTerrainData(true, {110, 120, 130, 140});
    QCOMPARE(resultSpy.count(), 2);
    QVERIFY(resultSpy.at(1).at(0).toBool());
    QCOMPARE(geo.elevationState(), Viewer3DTerrainGeometry::Ready);
    QCOMPARE(geo._elevations.size(), size_t(4));
    QCOMPARE(geo._vertices.front().z(), 10.0f);
    QCOMPARE(geo.elevationAt(QGeoCoordinate(35.91, 120.0)), 10.0);
    QCOMPARE(geo.elevationAt(QGeoCoordinate(35.89, 120.01)), 40.0);
    QVERIFY(std::isnan(geo.elevationAt(QGeoCoordinate(35.8, 120.0))));
    geo.setRoiMin(QGeoCoordinate(35.88, 120.0));
    QVERIFY(std::isnan(geo.elevationAt(QGeoCoordinate(35.9, 120.0))));
}

void Viewer3DTerrainGeometryTest::_testTerrainPreviewDatum()
{
    Viewer3DTerrainGeometry geo;
    geo.setSectorCount(2);
    geo.setStackCount(2);
    geo.setRoiMin(QGeoCoordinate(35.9, 120.0));
    geo.setRoiMax(QGeoCoordinate(35.91, 120.01));
    geo.setRefCoordinate(QGeoCoordinate(35.905, 120.005));
    geo.requestTerrainPreview();
    QVERIFY(geo._elevationQuery);
    geo._elevationQuery->signalTerrainData(true, {100, 110, 120, 130, 140, 150, 160, 170, 180});
    QCOMPARE(geo.elevationState(), Viewer3DTerrainGeometry::Ready);
    QCOMPARE(geo._referenceElevation, 140.0);
    QCOMPARE(geo.elevationAt(QGeoCoordinate(35.91, 120.0)), -40.0);
    geo.clearElevationData();
    QCOMPARE(geo.elevationState(), Viewer3DTerrainGeometry::Idle);
    QVERIFY(std::isnan(geo.elevationAt(QGeoCoordinate(35.91, 120.0))));
    for (const auto &vertex : geo._vertices) {
        QCOMPARE(vertex.z(), 0.0f);
    }
}

void Viewer3DTerrainGeometryTest::_testElevationQueryFailure_data()
{
    QTest::addColumn<bool>("success");
    QTest::addColumn<QList<double>>("heights");
    QTest::newRow("service-failed") << false << QList<double>{100, 110, 120, 130};
    QTest::newRow("missing-sample") << true << QList<double>{100, 110, 120};
    QTest::newRow("nodata") << true << QList<double>{100, qQNaN(), 120, 130};
    QTest::newRow("infinite") << true << QList<double>{100, INFINITY, 120, 130};
    QTest::newRow("out-of-range") << true << QList<double>{100, -32768, 120, 130};
}

void Viewer3DTerrainGeometryTest::_testElevationQueryFailure()
{
    QFETCH(bool, success);
    QFETCH(QList<double>, heights);
    Viewer3DTerrainGeometry geo;
    geo.setSectorCount(1);
    geo.setStackCount(1);
    geo.setRoiMin(QGeoCoordinate(35.9, 120.0));
    geo.setRoiMax(QGeoCoordinate(35.91, 120.01));
    geo.setRefCoordinate(QGeoCoordinate(35.905, 120.005));
    geo.requestElevationData(100);
    QVERIFY(geo._elevationQuery);
    QSignalSpy resultSpy(&geo, &Viewer3DTerrainGeometry::elevationDataReceived);
    geo._elevationQuery->signalTerrainData(success, heights);
    QCOMPARE(geo.elevationState(), Viewer3DTerrainGeometry::Failed);
    QCOMPARE(resultSpy.count(), 1);
    QVERIFY(!resultSpy.first().first().toBool());
    QVERIFY(!geo._elevationTimeout->isActive());
    QVERIFY(std::isnan(geo.elevationAt(QGeoCoordinate(35.91, 120.0))));
    for (const auto &vertex : geo._vertices) {
        QCOMPARE(vertex.z(), 0.0f);
    }
}

void Viewer3DTerrainGeometryTest::_testElevationQueryTimeout()
{
    Viewer3DTerrainGeometry geo;
    geo.setSectorCount(1);
    geo.setStackCount(1);
    geo.setRoiMin(QGeoCoordinate(35.9, 120.0));
    geo.setRoiMax(QGeoCoordinate(35.91, 120.01));
    geo.setRefCoordinate(QGeoCoordinate(35.905, 120.005));
    geo.requestElevationData(100);
    auto *query = geo._elevationQuery;
    QVERIFY(query);
    QVERIFY(geo._elevationTimeout->isActive());
    QSignalSpy resultSpy(&geo, &Viewer3DTerrainGeometry::elevationDataReceived);
    QVERIFY(QMetaObject::invokeMethod(geo._elevationTimeout, "timeout", Qt::DirectConnection));
    QCOMPARE(geo.elevationState(), Viewer3DTerrainGeometry::Failed);
    QVERIFY(!geo._elevationQuery);
    QVERIFY(!geo._elevationTimeout->isActive());
    query->signalTerrainData(true, {110, 120, 130, 140});
    QVERIFY(geo._elevations.empty());
    QCOMPARE(resultSpy.count(), 1);
    QVERIFY(!resultSpy.at(0).at(0).toBool());
}

void Viewer3DTerrainGeometryTest::_testElevationInterpolation()
{
    Viewer3DTerrainGeometry geo;
    geo.setSectorCount(1);
    geo.setStackCount(1);
    geo.setRoiMin(QGeoCoordinate(35.9, 120.0));
    geo.setRoiMax(QGeoCoordinate(35.91, 120.01));
    geo._elevations = {100, 100, 100, 200};
    geo._referenceElevation = 100;
    geo._setElevationState(Viewer3DTerrainGeometry::Ready);
    const auto coordinate = [](double u, double v) {
        return QGeoCoordinate(Viewer3DTerrainGeometry::_gridLatitude(35.91, 35.9, v * 4, 4),
                              120.0 + 0.01 * u);
    };
    // A bilinear interpolation would incorrectly return 25 at the diagonal center.
    QVERIFY(std::abs(geo.elevationAt(coordinate(0.5, 0.5))) < 1e-6);
    QVERIFY(std::abs(geo.elevationAt(coordinate(0.75, 0.75)) - 50) < 1e-6);
    QVERIFY(std::abs(geo.elevationAt(coordinate(0.25, 0.25))) < 1e-6);
}

void Viewer3DTerrainGeometryTest::_testMercatorGridLatitude()
{
    const double north = 60;
    const double south = 30;
    const double topY = std::asinh(std::tan(qDegreesToRadians(north)));
    const double bottomY = std::asinh(std::tan(qDegreesToRadians(south)));
    for (int row = 0; row <= 4; ++row) {
        const double latitude = Viewer3DTerrainGeometry::_gridLatitude(north, south, row, 4);
        const double y = std::asinh(std::tan(qDegreesToRadians(latitude)));
        QVERIFY(std::abs(y - (topY + (bottomY - topY) * row / 4)) < 1e-10);
    }
    QVERIFY(std::abs(Viewer3DTerrainGeometry::_gridLatitude(north, south, 2, 4) - 45) > 1);
}

void Viewer3DTerrainGeometryTest::_testElevationSamples()
{
    Viewer3DTerrainGeometry geo;
    geo.setSectorCount(1);
    geo.setStackCount(1);
    const std::array<double, 4> heights = {95, 110, 120, 140};
    QVERIFY(geo._buildTerrain(QGeoCoordinate(35.9, 120.0), QGeoCoordinate(35.91, 120.01),
                              QGeoCoordinate(35.905, 120.005), true, heights, 100));
    QCOMPARE(geo._vertices.size(), size_t(6));
    // Each output vertex must retain the sample for its grid corner.
    for (const auto &vertex : geo._vertices) {
        const bool north = vertex.y() > 0;
        const bool east = vertex.x() > 0;
        const size_t index = (north ? 0 : 2) + (east ? 1 : 0);
        QCOMPARE(vertex.z(), static_cast<float>(heights[index] - 100));
    }
}

void Viewer3DTerrainGeometryTest::_testInvalidElevationSamples()
{
    Viewer3DTerrainGeometry geo;
    geo.setSectorCount(1);
    geo.setStackCount(1);
    const QGeoCoordinate min(35.9, 120.0);
    const QGeoCoordinate max(35.91, 120.01);
    const QGeoCoordinate ref(35.905, 120.005);
    const std::array<double, 3> incomplete = {1, 2, 3};
    QVERIFY(!geo._buildTerrain(min, max, ref, true, incomplete, 0));
    const std::array<double, 4> invalid = {1, 2, std::numeric_limits<double>::quiet_NaN(), 4};
    QVERIFY(!geo._buildTerrain(min, max, ref, true, invalid, 0));
    const std::array<double, 4> valid = {1, 2, 3, 4};
    QVERIFY(!geo._buildTerrain(min, max, ref, true, valid, std::numeric_limits<double>::infinity()));
}

void Viewer3DTerrainGeometryTest::_testClearScene()
{
    Viewer3DTerrainGeometry geo;

    geo.setSectorCount(2);
    geo.setStackCount(2);

    QGeoCoordinate roiMin(47.0, 8.0, 0);
    QGeoCoordinate roiMax(47.01, 8.01, 0);
    QGeoCoordinate ref(47.005, 8.005, 0);

    geo._buildTerrain(roiMin, roiMax, ref, true);
    QVERIFY(!geo._vertices.empty());

    geo._clearScene();

    QVERIFY(geo._vertices.empty());
    QVERIFY(geo._normals.empty());
    QVERIFY(geo._texCoords.empty());
    QCOMPARE(geo.sectorCount(), 0);
    QCOMPARE(geo.stackCount(), 0);
}

void Viewer3DTerrainGeometryTest::_testComputeFaceNormalNegativeZ()
{
    Viewer3DTerrainGeometry geo;

    // Reversed winding: normal should point in -Z
    QVector3D x1(0, 0, 0);
    QVector3D x2(0, 1, 0);
    QVector3D x3(1, 0, 0);

    QVector3D normal = geo._computeFaceNormal(x1, x2, x3);

    QCOMPARE_FUZZY(normal.x(), 0.0f, 0.0001f);
    QCOMPARE_FUZZY(normal.y(), 0.0f, 0.0001f);
    QCOMPARE_FUZZY(normal.z(), -1.0f, 0.0001f);
}

void Viewer3DTerrainGeometryTest::_testBuildTerrainVertexCountScaling()
{
    Viewer3DTerrainGeometry geo;

    QGeoCoordinate roiMin(47.0, 8.0, 0);
    QGeoCoordinate roiMax(47.01, 8.01, 0);
    QGeoCoordinate ref(47.005, 8.005, 0);

    // 2x2 grid
    geo.setSectorCount(2);
    geo.setStackCount(2);
    geo._buildTerrain(roiMin, roiMax, ref, true);
    const size_t count2x2 = geo._vertices.size();

    // 4x4 grid should produce more vertices
    geo._vertices.clear();
    geo._normals.clear();
    geo._texCoords.clear();
    geo.setSectorCount(4);
    geo.setStackCount(4);
    geo._buildTerrain(roiMin, roiMax, ref, true);
    const size_t count4x4 = geo._vertices.size();

    QVERIFY(count4x4 > count2x2);
}

void Viewer3DTerrainGeometryTest::_testRoiSetters()
{
    Viewer3DTerrainGeometry geo;

    {
        QSignalSpy spy(&geo, &Viewer3DTerrainGeometry::roiMinChanged);
        QGeoCoordinate min(47.0, 8.0, 0);
        geo.setRoiMin(min);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(geo.roiMin(), min);

        // Same value: no signal
        geo.setRoiMin(min);
        QCOMPARE(spy.count(), 1);
    }

    {
        QSignalSpy spy(&geo, &Viewer3DTerrainGeometry::roiMaxChanged);
        QGeoCoordinate max(47.1, 8.1, 0);
        geo.setRoiMax(max);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(geo.roiMax(), max);

        geo.setRoiMax(max);
        QCOMPARE(spy.count(), 1);
    }

    {
        QSignalSpy spy(&geo, &Viewer3DTerrainGeometry::refCoordinateChanged);
        QGeoCoordinate ref(47.05, 8.05, 0);
        geo.setRefCoordinate(ref);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(geo.refCoordinate(), ref);

        geo.setRefCoordinate(ref);
        QCOMPARE(spy.count(), 1);
    }
}

void Viewer3DTerrainGeometryTest::_testPropertySetters()
{
    Viewer3DTerrainGeometry geo;

    {
        QSignalSpy spy(&geo, &Viewer3DTerrainGeometry::sectorCountChanged);
        geo.setSectorCount(10);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(geo.sectorCount(), 10);

        geo.setSectorCount(10);
        QCOMPARE(spy.count(), 1);
    }

    {
        QSignalSpy spy(&geo, &Viewer3DTerrainGeometry::stackCountChanged);
        geo.setStackCount(20);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(geo.stackCount(), 20);

        geo.setStackCount(20);
        QCOMPARE(spy.count(), 1);
    }
}

UT_REGISTER_TEST(Viewer3DTerrainGeometryTest, TestLabel::Unit)
