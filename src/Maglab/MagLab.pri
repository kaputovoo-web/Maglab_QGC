# ============================================================================
#  MagLab — custom feature build unit.
#
#  All MagLab source files are registered here so that the main
#  qgroundcontrol.pro only needs a single `include(src/MagLab/MagLab.pri)`.
# ============================================================================

INCLUDEPATH += \
    $$PWD \
    $$PWD/Archive \
    $$PWD/MagMap \
    $$PWD/Planning/LLMPlan \
    $$PWD/Planning/Sketch \
    $$PWD/Settings \
    $$PWD/Survey \
    $$PWD/Survey/SimpleTest \
    $$PWD/Video \
    $$PWD/Video/RTSPCapture \
    $$PWD/Attitude \
    $$PWD/Attitude/Yaw \
    $$PWD/Common \

HEADERS += \
    $$PWD/Common/MagLabPaths.h \
    $$PWD/Survey/SimpleTest/SimpleTest.h \
    $$PWD/Survey/SimpleTest/DataRecorder.h \
    $$PWD/Survey/SimpleTest/Handle.h \
    $$PWD/Video/RTSPCapture/RtspCapture.h \
    $$PWD/Attitude/Yaw/AttitudeAnglesController.h \
    $$PWD/MagMap/MagneticMapController.h \
    $$PWD/Planning/LLMPlan/LLMComplexItem.h \
    $$PWD/Planning/LLMPlan/LLMPlanCreator.h \
    $$PWD/Planning/LLMPlan/LLMPlanStrategy.h \
    $$PWD/Planning/LLMPlan/SpiralPlanStrategy.h \
    $$PWD/Planning/LLMPlan/CustomPlanStrategy.h \
    $$PWD/Planning/LLMPlan/FomPlanStrategy.h \
    $$PWD/Planning/LLMPlan/SquarePlanStrategy.h \
    $$PWD/Planning/LLMPlan/VoicePlanStrategy.h \
    $$PWD/Planning/Sketch/SketchComplexItem.h \
    $$PWD/Planning/Sketch/SketchPlanCreator.h \
    $$PWD/Archive/MissionArchiveManager.h \
    $$PWD/Settings/LLMSettings.h

SOURCES += \
    $$PWD/Common/MagLabPaths.cc \
    $$PWD/Survey/SimpleTest/SimpleTest.cpp \
    $$PWD/Survey/SimpleTest/DataRecorder.cpp \
    $$PWD/Survey/SimpleTest/Handle.cpp \
    $$PWD/Video/RTSPCapture/RtspCapture.cpp \
    $$PWD/Attitude/Yaw/AttitudeAnglesController.cc \
    $$PWD/MagMap/MagneticMapController.cc \
    $$PWD/Planning/LLMPlan/LLMComplexItem.cc \
    $$PWD/Planning/LLMPlan/LLMPlanCreator.cc \
    $$PWD/Planning/LLMPlan/SpiralPlanStrategy.cc \
    $$PWD/Planning/LLMPlan/CustomPlanStrategy.cc \
    $$PWD/Planning/LLMPlan/FomPlanStrategy.cc \
    $$PWD/Planning/LLMPlan/SquarePlanStrategy.cc \
    $$PWD/Planning/LLMPlan/VoicePlanStrategy.cc \
    $$PWD/Planning/Sketch/SketchComplexItem.cc \
    $$PWD/Planning/Sketch/SketchPlanCreator.cc \
    $$PWD/Archive/MissionArchiveManager.cc \
    $$PWD/Settings/LLMSettings.cc

# ----------------------------------------------------------------------------
#  RTSP capture (RtspCapture) depends on ffmpeg.
#   - Linux desktop: system ffmpeg (4.4 / libavcodec 58) via pkg-config
#   - Android: the prebuilt arm64-v8a ffmpeg bundled under Video/RTSPCapture/ffmpeg
#     (ffmpeg 8 / libavcodec 62). RtspCapture.cpp guards the difference with
#     #ifdef AV_FRAME_FLAG_KEY so one source tree builds for both.
LinuxBuild {
    CONFIG    += link_pkgconfig
    PKGCONFIG += libavformat libavcodec libavutil libswscale libswresample
}

AndroidBuild {
    INCLUDEPATH += $$PWD/Video/RTSPCapture/ffmpeg/include
    LIBS += -L$$PWD/Video/RTSPCapture/ffmpeg/lib/arm64-v8a \
        -lavformat -lavcodec -lswscale -lswresample -lavutil
}
