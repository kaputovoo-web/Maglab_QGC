#include "GeoCameraProjector.h"

#include <cmath>
#include <algorithm>

namespace
{
    constexpr double WGS84_A = 6378137.0;
    constexpr double WGS84_F = 1.0 / 298.257223563;
    constexpr double WGS84_E2 = WGS84_F * (2.0 - WGS84_F);

    constexpr double EPS = 1e-12;
}

GeoCameraProjector::CameraIntrinsics GeoCameraProjector::intrinsicsFromFov(
    int imageWidth,
    int imageHeight,
    double hfovDeg,
    double vfovDeg)
{
    CameraIntrinsics K;
    K.imageWidth = imageWidth;
    K.imageHeight = imageHeight;

    const double hfovRad = qDegreesToRadians(hfovDeg);
    const double vfovRad = qDegreesToRadians(vfovDeg);

    K.fx = (imageWidth * 0.5) / std::tan(hfovRad * 0.5);
    K.fy = (imageHeight * 0.5) / std::tan(vfovRad * 0.5);
    K.cx = imageWidth * 0.5;
    K.cy = imageHeight * 0.5;

    return K;
}

bool GeoCameraProjector::geoToPixelByPose(
    double targetLatDeg,
    double targetLonDeg,
    double targetAltM,
    const CameraPose& pose,
    const CameraIntrinsics& K,
    QPointF& pixelOut)
{
    if (K.imageWidth <= 0 || K.imageHeight <= 0)
        return false;

    if (std::abs(K.fx) < EPS || std::abs(K.fy) < EPS)
        return false;

    // 1. 地面点经纬度 -> ENU
    Vec3 targetEnu = geodeticToEnu(
        targetLatDeg,
        targetLonDeg,
        targetAltM,
        pose.refLatDeg,
        pose.refLonDeg,
        pose.refAltM);

    // 2. 相机中心在 ENU 中的位置
    Vec3 cameraCenterEnu = makeVec(0.0, 0.0, pose.heightAGL);

    // 3. 世界向量：相机中心 -> 目标点
    Vec3 dirEnu = sub(targetEnu, cameraCenterEnu);

    // 4. ENU -> Camera
    Mat3 R_cam_to_enu = cameraToEnuRotation(pose);
    Mat3 R_enu_to_cam = transpose(R_cam_to_enu);

    Vec3 dirCam = multiply(R_enu_to_cam, dirEnu);

    // z_cam <= 0 表示点在相机后方或者光轴反方向
    if (dirCam.z <= EPS)
        return false;

    // 5. 针孔投影
    const double u = K.fx * (dirCam.x / dirCam.z) + K.cx;
    const double v = K.fy * (dirCam.y / dirCam.z) + K.cy;

    pixelOut = QPointF(u, v);
    return true;
}

bool GeoCameraProjector::pixelToGeoByPose(
    double u,
    double v,
    double groundAltM,
    const CameraPose& pose,
    const CameraIntrinsics& K,
    GeoPoint& geoOut)
{
    if (K.imageWidth <= 0 || K.imageHeight <= 0)
        return false;

    if (std::abs(K.fx) < EPS || std::abs(K.fy) < EPS)
        return false;

    // 1. 像素 -> 相机坐标系射线
    Vec3 rayCam = makeVec(
        (u - K.cx) / K.fx,
        (v - K.cy) / K.fy,
        1.0);

    // 2. Camera -> ENU
    Mat3 R_cam_to_enu = cameraToEnuRotation(pose);
    Vec3 rayEnu = multiply(R_cam_to_enu, rayCam);

    // 3. 相机中心在 ENU 中的位置
    Vec3 cameraCenterEnu = makeVec(0.0, 0.0, pose.heightAGL);

    // 4. 地面高程对应的 ENU z
    // 因为 ENU 原点高程是 pose.refAltM，所以地面平面 z 为：
    const double groundZ = groundAltM - pose.refAltM;

    // 射线：
    // P = C + t * ray
    //
    // 与 z = groundZ 求交：
    // C.z + t * ray.z = groundZ
    if (std::abs(rayEnu.z) < EPS)
        return false;

    const double t = (groundZ - cameraCenterEnu.z) / rayEnu.z;

    // t <= 0 表示交点在相机后方
    if (t <= EPS)
        return false;

    Vec3 groundEnu = add(cameraCenterEnu, mul(rayEnu, t));

    Vec3 groundEcef = enuToEcef(
        groundEnu,
        pose.refLatDeg,
        pose.refLonDeg,
        pose.refAltM);

    geoOut = ecefToGeodetic(groundEcef);
    return true;
}

bool GeoCameraProjector::imageGroundCorners(
    const CameraPose& pose,
    const CameraIntrinsics& K,
    GeoPoint cornersOut[4])
{
    if (!cornersOut)
        return false;

    const double groundAltM = pose.refAltM;

    bool ok = true;

    // 左上
    ok = ok && pixelToGeoByPose(
        0.0,
        0.0,
        groundAltM,
        pose,
        K,
        cornersOut[0]);

    // 左下
    ok = ok && pixelToGeoByPose(
        0.0,
        K.imageHeight - 1.0,
        groundAltM,
        pose,
        K,
        cornersOut[1]);

    // 右下
    ok = ok && pixelToGeoByPose(
        K.imageWidth - 1.0,
        K.imageHeight - 1.0,
        groundAltM,
        pose,
        K,
        cornersOut[2]);

    // 右上
    ok = ok && pixelToGeoByPose(
        K.imageWidth - 1.0,
        0.0,
        groundAltM,
        pose,
        K,
        cornersOut[3]);

    return ok;
}

GeoCameraProjector::Vec3 GeoCameraProjector::makeVec(double x, double y, double z)
{
    Vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

GeoCameraProjector::Vec3 GeoCameraProjector::add(const Vec3& a, const Vec3& b)
{
    return makeVec(a.x + b.x, a.y + b.y, a.z + b.z);
}

GeoCameraProjector::Vec3 GeoCameraProjector::sub(const Vec3& a, const Vec3& b)
{
    return makeVec(a.x - b.x, a.y - b.y, a.z - b.z);
}

GeoCameraProjector::Vec3 GeoCameraProjector::mul(const Vec3& a, double s)
{
    return makeVec(a.x * s, a.y * s, a.z * s);
}

double GeoCameraProjector::dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

GeoCameraProjector::Mat3 GeoCameraProjector::identity()
{
    Mat3 I;
    I.m[0][0] = 1.0;
    I.m[1][1] = 1.0;
    I.m[2][2] = 1.0;
    return I;
}

GeoCameraProjector::Mat3 GeoCameraProjector::transpose(const Mat3& A)
{
    Mat3 B;

    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            B.m[r][c] = A.m[c][r];
        }
    }

    return B;
}

GeoCameraProjector::Mat3 GeoCameraProjector::multiply(const Mat3& A, const Mat3& B)
{
    Mat3 C;

    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            C.m[r][c] = 0.0;

            for (int k = 0; k < 3; ++k)
            {
                C.m[r][c] += A.m[r][k] * B.m[k][c];
            }
        }
    }

    return C;
}

GeoCameraProjector::Vec3 GeoCameraProjector::multiply(const Mat3& A, const Vec3& v)
{
    return makeVec(
        A.m[0][0] * v.x + A.m[0][1] * v.y + A.m[0][2] * v.z,
        A.m[1][0] * v.x + A.m[1][1] * v.y + A.m[1][2] * v.z,
        A.m[2][0] * v.x + A.m[2][1] * v.y + A.m[2][2] * v.z);
}

GeoCameraProjector::Mat3 GeoCameraProjector::rotX(double rad)
{
    const double c = std::cos(rad);
    const double s = std::sin(rad);

    Mat3 R = identity();
    R.m[1][1] = c;
    R.m[1][2] = -s;
    R.m[2][1] = s;
    R.m[2][2] = c;

    return R;
}

GeoCameraProjector::Mat3 GeoCameraProjector::rotY(double rad)
{
    const double c = std::cos(rad);
    const double s = std::sin(rad);

    Mat3 R = identity();
    R.m[0][0] = c;
    R.m[0][2] = s;
    R.m[2][0] = -s;
    R.m[2][2] = c;

    return R;
}

GeoCameraProjector::Mat3 GeoCameraProjector::rotZ(double rad)
{
    const double c = std::cos(rad);
    const double s = std::sin(rad);

    Mat3 R = identity();
    R.m[0][0] = c;
    R.m[0][1] = -s;
    R.m[1][0] = s;
    R.m[1][1] = c;

    return R;
}

GeoCameraProjector::Vec3 GeoCameraProjector::geodeticToEcef(
    double latDeg,
    double lonDeg,
    double altM)
{
    const double lat = qDegreesToRadians(latDeg);
    const double lon = qDegreesToRadians(lonDeg);

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);

    const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    const double x = (N + altM) * cosLat * cosLon;
    const double y = (N + altM) * cosLat * sinLon;
    const double z = (N * (1.0 - WGS84_E2) + altM) * sinLat;

    return makeVec(x, y, z);
}

GeoCameraProjector::GeoPoint GeoCameraProjector::ecefToGeodetic(const Vec3& ecef)
{
    // Bowring 迭代法，精度足够用于无人机局部映射。
    const double x = ecef.x;
    const double y = ecef.y;
    const double z = ecef.z;

    const double b = WGS84_A * (1.0 - WGS84_F);
    const double ep2 = (WGS84_A * WGS84_A - b * b) / (b * b);

    const double p = std::sqrt(x * x + y * y);
    const double theta = std::atan2(z * WGS84_A, p * b);

    const double sinTheta = std::sin(theta);
    const double cosTheta = std::cos(theta);

    const double lat = std::atan2(
        z + ep2 * b * sinTheta * sinTheta * sinTheta,
        p - WGS84_E2 * WGS84_A * cosTheta * cosTheta * cosTheta);

    const double lon = std::atan2(y, x);

    const double sinLat = std::sin(lat);
    const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    const double alt = p / std::cos(lat) - N;

    GeoPoint geo;
    geo.latDeg = qRadiansToDegrees(lat);
    geo.lonDeg = qRadiansToDegrees(lon);
    geo.altM = alt;

    return geo;
}

GeoCameraProjector::Mat3 GeoCameraProjector::ecefToEnuMatrix(
    double refLatDeg,
    double refLonDeg)
{
    const double lat = qDegreesToRadians(refLatDeg);
    const double lon = qDegreesToRadians(refLonDeg);

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);

    Mat3 R;

    // ECEF -> ENU
    R.m[0][0] = -sinLon;
    R.m[0][1] =  cosLon;
    R.m[0][2] =  0.0;

    R.m[1][0] = -sinLat * cosLon;
    R.m[1][1] = -sinLat * sinLon;
    R.m[1][2] =  cosLat;

    R.m[2][0] =  cosLat * cosLon;
    R.m[2][1] =  cosLat * sinLon;
    R.m[2][2] =  sinLat;

    return R;
}

GeoCameraProjector::Vec3 GeoCameraProjector::geodeticToEnu(
    double latDeg,
    double lonDeg,
    double altM,
    double refLatDeg,
    double refLonDeg,
    double refAltM)
{
    Vec3 ecef = geodeticToEcef(latDeg, lonDeg, altM);
    Vec3 refEcef = geodeticToEcef(refLatDeg, refLonDeg, refAltM);

    Vec3 diff = sub(ecef, refEcef);

    Mat3 R_ecef_to_enu = ecefToEnuMatrix(refLatDeg, refLonDeg);

    return multiply(R_ecef_to_enu, diff);
}

GeoCameraProjector::Vec3 GeoCameraProjector::enuToEcef(
    const Vec3& enu,
    double refLatDeg,
    double refLonDeg,
    double refAltM)
{
    Vec3 refEcef = geodeticToEcef(refLatDeg, refLonDeg, refAltM);

    Mat3 R_ecef_to_enu = ecefToEnuMatrix(refLatDeg, refLonDeg);
    Mat3 R_enu_to_ecef = transpose(R_ecef_to_enu);

    Vec3 diffEcef = multiply(R_enu_to_ecef, enu);

    return add(refEcef, diffEcef);
}

GeoCameraProjector::Mat3 GeoCameraProjector::cameraToEnuRotation(
    const CameraPose& pose)
{
    const double roll = qDegreesToRadians(pose.rollDeg);
    const double pitch = qDegreesToRadians(pose.pitchDeg);
    const double yaw = qDegreesToRadians(pose.yawDeg);

    // 机体系 body:
    // x_body = forward
    // y_body = right
    // z_body = down
    //
    // NED:
    // x_ned = north
    // y_ned = east
    // z_ned = down
    //
    // 常见航空姿态：
    // R_body_to_ned = Rz(yaw) * Ry(pitch) * Rx(roll)

    Mat3 R_body_to_ned = multiply(
        multiply(rotZ(yaw), rotY(pitch)),
        rotX(roll));

    // Camera -> Body
    //
    // x_cam = image right     -> y_body
    // y_cam = image down      -> -x_body
    // z_cam = optical forward -> z_body
    //
    // 所以 body 中的三个 camera 基向量为：
    //
    // x_cam in body = ( 0, 1, 0)
    // y_cam in body = (-1, 0, 0)
    // z_cam in body = ( 0, 0, 1)

    Mat3 R_cam_to_body;

    R_cam_to_body.m[0][0] =  0.0;
    R_cam_to_body.m[1][0] =  1.0;
    R_cam_to_body.m[2][0] =  0.0;

    R_cam_to_body.m[0][1] = -1.0;
    R_cam_to_body.m[1][1] =  0.0;
    R_cam_to_body.m[2][1] =  0.0;

    R_cam_to_body.m[0][2] =  0.0;
    R_cam_to_body.m[1][2] =  0.0;
    R_cam_to_body.m[2][2] =  1.0;

    Mat3 R_cam_to_ned = multiply(R_body_to_ned, R_cam_to_body);

    // NED -> ENU
    //
    // enu.x = ned.y
    // enu.y = ned.x
    // enu.z = -ned.z

    Mat3 R_ned_to_enu;

    R_ned_to_enu.m[0][0] = 0.0;
    R_ned_to_enu.m[0][1] = 1.0;
    R_ned_to_enu.m[0][2] = 0.0;

    R_ned_to_enu.m[1][0] = 1.0;
    R_ned_to_enu.m[1][1] = 0.0;
    R_ned_to_enu.m[1][2] = 0.0;

    R_ned_to_enu.m[2][0] = 0.0;
    R_ned_to_enu.m[2][1] = 0.0;
    R_ned_to_enu.m[2][2] = -1.0;

    Mat3 R_cam_to_enu = multiply(R_ned_to_enu, R_cam_to_ned);

    return R_cam_to_enu;
}
