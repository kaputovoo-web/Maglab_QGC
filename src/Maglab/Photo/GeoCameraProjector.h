#pragma once

#include <QtGlobal>
#include <QPointF>
#include <QtMath>

class GeoCameraProjector
{
public:
    struct GeoPoint
    {
        double latDeg = 0.0;
        double lonDeg = 0.0;
        double altM = 0.0;
    };

    struct CameraIntrinsics
    {
        int imageWidth = 0;
        int imageHeight = 0;

        double fx = 0.0;
        double fy = 0.0;
        double cx = 0.0;
        double cy = 0.0;
    };

    struct CameraPose
    {
        // 俯拍参考点，经纬度。
        // 一般取无人机 GPS 水平位置在地面的投影点。
        double refLatDeg = 0.0;
        double refLonDeg = 0.0;

        // 参考地面高程。
        // 如果你只有相对高度，可以先设为 0。
        double refAltM = 0.0;

        // 相机中心相对参考地面的高度。
        // 也就是 AGL，高度 above ground level。
        double heightAGL = 0.0;

        // 无人机姿态角，单位 degree。
        // roll  : 右倾为正
        // pitch : 抬头为正
        // yaw   : 北为 0，顺时针为正
        double rollDeg = 0.0;
        double pitchDeg = 0.0;
        double yawDeg = 0.0;
    };

    static CameraIntrinsics intrinsicsFromFov(
        int imageWidth,
        int imageHeight,
        double hfovDeg,
        double vfovDeg);

    // 地面经纬度 -> 图像像素。
    // 返回 false 表示点在相机后方或参数非法。
    static bool geoToPixelByPose(
        double targetLatDeg,
        double targetLonDeg,
        double targetAltM,
        const CameraPose& pose,
        const CameraIntrinsics& K,
        QPointF& pixelOut);

    // 图像像素 -> 地面经纬度。
    // 默认和 z = groundAltM 的水平地面平面求交。
    // 如果你没有地面高程，groundAltM 可以传 pose.refAltM。
    static bool pixelToGeoByPose(
        double u,
        double v,
        double groundAltM,
        const CameraPose& pose,
        const CameraIntrinsics& K,
        GeoPoint& geoOut);

    // 计算整张图四个角对应的地面经纬度。
    // 顺序：左上、左下、右下、右上。
    static bool imageGroundCorners(
        const CameraPose& pose,
        const CameraIntrinsics& K,
        GeoPoint cornersOut[4]);

private:
    struct Vec3
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    struct Mat3
    {
        double m[3][3] = {{0.0, 0.0, 0.0},
                          {0.0, 0.0, 0.0},
                          {0.0, 0.0, 0.0}};
    };

private:
    static Vec3 makeVec(double x, double y, double z);

    static Vec3 add(const Vec3& a, const Vec3& b);
    static Vec3 sub(const Vec3& a, const Vec3& b);
    static Vec3 mul(const Vec3& a, double s);
    static double dot(const Vec3& a, const Vec3& b);

    static Mat3 identity();
    static Mat3 transpose(const Mat3& A);
    static Mat3 multiply(const Mat3& A, const Mat3& B);
    static Vec3 multiply(const Mat3& A, const Vec3& v);

    static Mat3 rotX(double rad);
    static Mat3 rotY(double rad);
    static Mat3 rotZ(double rad);

    static Vec3 geodeticToEcef(double latDeg, double lonDeg, double altM);
    static GeoPoint ecefToGeodetic(const Vec3& ecef);

    static Mat3 ecefToEnuMatrix(double refLatDeg, double refLonDeg);
    static Vec3 geodeticToEnu(
        double latDeg,
        double lonDeg,
        double altM,
        double refLatDeg,
        double refLonDeg,
        double refAltM);

    static Vec3 enuToEcef(
        const Vec3& enu,
        double refLatDeg,
        double refLonDeg,
        double refAltM);

    // 相机坐标 -> ENU 坐标的旋转矩阵。
    static Mat3 cameraToEnuRotation(const CameraPose& pose);
};
