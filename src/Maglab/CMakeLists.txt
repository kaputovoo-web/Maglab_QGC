# Migrate independently verified backends first. Planning/UI integration follows separately.
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Attitude/Yaw/AttitudeAnglesController.cc
    Attitude/Yaw/AttitudeAnglesController.h
    Archive/MissionArchiveManager.cc
    Archive/MissionArchiveManager.h
    Survey/SimpleTest/Handle.cpp
    Survey/SimpleTest/Handle.h
    Survey/SimpleTest/SimpleTest.cpp
    Survey/SimpleTest/SimpleTest.h
    Survey/SimpleTest/ScalarFrameParser.h
    Common/SingletonTest/SingletonTest.cc
    Common/SingletonTest/SingletonTest.h
    Planning/LLMPlan/LLMPlanCreator.cc
    Planning/LLMPlan/LLMPlanCreator.h
    Common/MagLabPaths.cc
    Common/MagLabPaths.h
    MagMap/MagneticMapController.cc
    MagMap/MagneticMapController.h
    Photo/GeoCameraProjector.cpp
    Photo/GeoCameraProjector.h
    Photo/PhotoOverlayController.cc
    Photo/PhotoOverlayController.h
    Survey/SimpleTest/DataRecorder.cpp
    Survey/SimpleTest/DataRecorder.h
    Survey/MagSurveySettings.cc
    Survey/MagSurveySettings.h
    Planning/Sketch/SketchComplexItem.cc
    Planning/Sketch/SketchComplexItem.h
    Planning/Sketch/SketchPlanCreator.cc
    Planning/Sketch/SketchPlanCreator.h
    Planning/LLMPlan/LLMComplexItem.cc
    Planning/LLMPlan/LLMComplexItem.h
    Planning/LLMPlan/VoicePlanStrategy.cc
    Planning/LLMPlan/VoicePlanStrategy.h
    Planning/LLMPlan/VoiceAudioRecorder.cc
    Planning/LLMPlan/VoiceAudioRecorder.h
    Planning/LLMPlan/LLMPlanStrategy.h
    Planning/LLMPlan/SpiralPlanStrategy.cc
    Planning/LLMPlan/SpiralPlanStrategy.h
    Planning/LLMPlan/SpiralMissionItemBuilder.cc
    Planning/LLMPlan/SpiralMissionItemBuilder.h
    Planning/LLMPlan/FomPlanStrategy.cc
    Planning/LLMPlan/FomPlanStrategy.h
    Planning/LLMPlan/CustomPlanStrategy.cc
    Planning/LLMPlan/CustomPlanStrategy.h
    Planning/LLMPlan/SquarePlanStrategy.cc
    Planning/LLMPlan/SquarePlanStrategy.h
    Settings/LLMSettings.cc
    Settings/LLMSettings.h
    Flight/InFlightMissionReplaceController.cc
    Flight/InFlightMissionReplaceController.h
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Common
    ${CMAKE_CURRENT_SOURCE_DIR}/Common/SingletonTest
    ${CMAKE_CURRENT_SOURCE_DIR}/Archive
    ${CMAKE_CURRENT_SOURCE_DIR}/Settings
    ${CMAKE_CURRENT_SOURCE_DIR}/Survey
    ${CMAKE_CURRENT_SOURCE_DIR}/Survey/Scalar
    ${CMAKE_CURRENT_SOURCE_DIR}/Survey/Vector
    ${CMAKE_CURRENT_SOURCE_DIR}/Survey/SimpleTest
    ${CMAKE_CURRENT_SOURCE_DIR}/Attitude/Yaw
    ${CMAKE_CURRENT_SOURCE_DIR}/MagMap
    ${CMAKE_CURRENT_SOURCE_DIR}/Photo
    ${CMAKE_CURRENT_SOURCE_DIR}/Planning/LLMPlan
    ${CMAKE_CURRENT_SOURCE_DIR}/Planning/Sketch
    ${CMAKE_CURRENT_SOURCE_DIR}/Flight
)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE MAGLAB_MODULES_ENABLED)

set_source_files_properties(MagMap/qml/MagneticMapView.qml PROPERTIES QT_RESOURCE_ALIAS MagneticMapView.qml)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_map_resources
    PREFIX "/maglab"
    FILES MagMap/qml/MagneticMapView.qml
)

set(_maglab_planning_qml
    Planning/qml/LLMItemEditor.qml
    Planning/qml/LLMMapVisual.qml
    Planning/qml/SketchItemEditor.qml
    Planning/qml/SketchMapVisual.qml
)
foreach(_qml IN LISTS _maglab_planning_qml)
    get_filename_component(_name "${_qml}" NAME)
    set_source_files_properties(${_qml} PROPERTIES QT_RESOURCE_ALIAS "${_name}")
endforeach()
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_planning_resources
    PREFIX "/maglab"
    FILES ${_maglab_planning_qml}
)

set_source_files_properties(Flight/InFlightMissionReplacePanel.qml PROPERTIES QT_RESOURCE_ALIAS InFlightMissionReplacePanel.qml)
set_source_files_properties(Flight/FlightMagneticCurve.qml PROPERTIES QT_RESOURCE_ALIAS FlightMagneticCurve.qml)
set_source_files_properties(Flight/MissionProgressLine.qml PROPERTIES QT_RESOURCE_ALIAS MissionProgressLine.qml)
set_source_files_properties(Flight/ConfettiBurst.qml PROPERTIES QT_RESOURCE_ALIAS ConfettiBurst.qml)
set_source_files_properties(Flight/MissionRouteEntryArea.qml PROPERTIES QT_RESOURCE_ALIAS MissionRouteEntryArea.qml)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_flight_resources
    PREFIX "/maglab"
    FILES Flight/InFlightMissionReplacePanel.qml Flight/MissionRouteEntryArea.qml Flight/FlightMagneticCurve.qml Flight/MissionProgressLine.qml Flight/ConfettiBurst.qml
)

set_source_files_properties(Archive/qml/FlyMissionArchiveCard.qml PROPERTIES QT_RESOURCE_ALIAS FlyMissionArchiveCard.qml)
set_source_files_properties(Archive/qml/MissionArchivePanel.qml PROPERTIES QT_RESOURCE_ALIAS MissionArchivePanel.qml)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_archive_resources
    PREFIX "/maglab"
    FILES Archive/qml/MissionArchivePanel.qml Archive/qml/FlyMissionArchiveCard.qml
)

set_source_files_properties(Planning/LLMPlan/LLMPlanCreator.png PROPERTIES QT_RESOURCE_ALIAS LLMPlanCreator.png)
set_source_files_properties(Planning/Sketch/SketchPlanCreator.png PROPERTIES QT_RESOURCE_ALIAS SketchPlanCreator.png)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_creator_images
    PREFIX "/maglab"
    FILES Planning/LLMPlan/LLMPlanCreator.png Planning/Sketch/SketchPlanCreator.png
)

set_source_files_properties(Survey/SimpleTest/MediaCapturePage.qml PROPERTIES QT_RESOURCE_ALIAS MediaCapturePage.qml)
set_source_files_properties(ExecutionObserver.qml PROPERTIES QT_RESOURCE_ALIAS ExecutionObserver.qml)
set_source_files_properties(DiagnosticPage.qml PROPERTIES QT_RESOURCE_ALIAS DiagnosticPage.qml)
set_source_files_properties(Survey/SimpleTest/CollectionPage.qml PROPERTIES QT_RESOURCE_ALIAS CollectionPage.qml)
set_source_files_properties(Survey/SimpleTest/MagMapGalleryPage.qml PROPERTIES QT_RESOURCE_ALIAS MagMapGalleryPage.qml)
set_source_files_properties(MagLabPanel.qml PROPERTIES QT_RESOURCE_ALIAS MagLabPanel.qml)
set_source_files_properties(Compensation/qml/CompensationPage.qml PROPERTIES QT_RESOURCE_ALIAS CompensationPage.qml)
set_source_files_properties(Photo/qml/PhotoOverlayPage.qml PROPERTIES QT_RESOURCE_ALIAS PhotoOverlayPage.qml)
set_source_files_properties(Photo/qml/ZoomablePreview.qml PROPERTIES QT_RESOURCE_ALIAS ZoomablePreview.qml)
set_source_files_properties(Realtime/qml/RealtimePage.qml PROPERTIES QT_RESOURCE_ALIAS RealtimePage.qml)
set_source_files_properties(Images/MaglabIcon.svg PROPERTIES QT_RESOURCE_ALIAS MaglabIcon.svg)
set_source_files_properties(Images/heu.svg PROPERTIES QT_RESOURCE_ALIAS heu.svg)
set_source_files_properties(Images/xunxun.png PROPERTIES QT_RESOURCE_ALIAS xunxun.png)
set_source_files_properties(Images/hyh.png PROPERTIES QT_RESOURCE_ALIAS hyh.png)
set_source_files_properties(Images/heu_standard.png PROPERTIES QT_RESOURCE_ALIAS heu_standard.png)
set_source_files_properties(Images/maglab_splash.png PROPERTIES QT_RESOURCE_ALIAS maglab_splash.png)
set_source_files_properties(StartupBrandOverlay.qml PROPERTIES QT_RESOURCE_ALIAS StartupBrandOverlay.qml)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_collection_page
    PREFIX "/maglab"
    FILES Survey/SimpleTest/CollectionPage.qml Survey/SimpleTest/MediaCapturePage.qml
          Survey/SimpleTest/MagMapGalleryPage.qml MagLabPanel.qml StartupBrandOverlay.qml
          Images/MaglabIcon.svg Images/heu.svg Images/xunxun.png
          Images/hyh.png Images/heu_standard.png Images/maglab_splash.png
          Compensation/qml/CompensationPage.qml
          Photo/qml/PhotoOverlayPage.qml Photo/qml/ZoomablePreview.qml
          Realtime/qml/RealtimePage.qml
          DiagnosticPage.qml ExecutionObserver.qml
)

# Package under distinct names: the LLMSettings backend is a QML type in QGC.
set_source_files_properties(Settings/qml/LLMSettings.qml PROPERTIES QT_RESOURCE_ALIAS LLMSettingsPage.qml)
set_source_files_properties(Settings/qml/TaskNameTimeSettings.qml PROPERTIES QT_RESOURCE_ALIAS TaskNameTimeSettings.qml)
set_source_files_properties(Settings/qml/SurveySettingsPage.qml PROPERTIES QT_RESOURCE_ALIAS SurveySettingsPage.qml)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_settings_pages
    PREFIX "/maglab"
    FILES Settings/qml/LLMSettings.qml Settings/qml/TaskNameTimeSettings.qml Settings/qml/SurveySettingsPage.qml
)

set_source_files_properties(Settings/LLM.SettingsGroup.json PROPERTIES QT_RESOURCE_ALIAS LLM.SettingsGroup.json)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_settings_resources
    PREFIX "/json"
    FILES Settings/LLM.SettingsGroup.json
)
