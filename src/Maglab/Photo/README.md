# MagLab Photo Overlay

This module is the refactored replacement for the old `src/Maglab/magphoto`
prototype.

## What was removed

The old prototype contained:

- Qt 5 / `QtCharts` / `QtAndroid` imports that do not match the Qt 6 QGC build.
- `QTextStream::setCodec()` and an older FFmpeg frame API.
- A second independent RTSP/FFmpeg capture stack with detached threads and
  hard-coded storage paths.
- Duplicate projection and overlay code.

The useful projection math was moved into `GeoCameraProjector.{h,cpp}`. The
capture and composition logic was reimplemented in
`PhotoOverlayController.{h,cc}` and `qml/PhotoOverlayPage.qml`.

## Current data flow

1. Capture a frame from the QGC/GStreamer video pipeline (`VideoManager`).
2. Write `txtvis<photo-base>.txt` beside the photo:
   - line 1: `longitude,latitude`
   - line 2: `roll,pitch,yaw`
   - line 3: `altitude` (optional)
3. Use the magnetic map `txtmag<map-base>.txt` corner file:
   - NW, SW, SE, NE as `longitude,latitude`.
4. Project the four map corners into the photo using the camera FOV,
   AGL height, roll/pitch/yaw and yaw offset.
5. Warp the magnetic raster with `QTransform::quadToQuad`.
6. Save the composite JPEG in `MagLabPaths::photoOverlayDir()`.

## Storage

- Photos: `MagLabPaths::photoDir()`
- Photo pose sidecars: `txtvis*.txt` beside each photo
- Magnetic maps: `MagLabPaths::magMapDir()`
- Magnetic map corners: `txtmag*.txt`
- Composites: `MagLabPaths::photoOverlayDir()`

## Notes

- RTSP is handled by the existing QGC GStreamer video path; the old
  `RTSPCapture` FFmpeg implementation is intentionally not reused.
- FOV, AGL height, yaw offset and transparency are persisted in
  `QSettings` under `MagLab/PhotoOverlay`.
- The projection is an approximate flat-ground pinhole model. It is not a
  terrain-aware orthorectification and must not be used as a survey-grade
  geometric product without calibration.
