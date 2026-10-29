#pragma once

#include "UnitTest.h"

class Viewer3DTerrainGeometry;

class Viewer3DTerrainGeometryTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testComputeFaceNormal();
    void _testComputeFaceNormalDegenerate();
    void _testComputeFaceNormalNegativeZ();
    void _testBuildTerrainZeroCounts();
    void _testBuildTerrainValidRegion();
    void _testInvalidTerrainRegion_data();
    void _testInvalidTerrainRegion();
    void _testBuildTerrainVertexCountScaling();
    void _testElevationQueryInvalidation();
    void _testElevationQueryTimeout();
    void _testTerrainPreviewDatum();
    void _testElevationQueryFailure_data();
    void _testElevationQueryFailure();
    void _testMercatorGridLatitude();
    void _testElevationInterpolation();
    void _testElevationSamples();
    void _testInvalidElevationSamples();
    void _testClearScene();
    void _testPropertySetters();
    void _testRoiSetters();
};
