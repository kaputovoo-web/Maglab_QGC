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
    Survey/SimpleTest/DataRecorder.cpp
    Survey/SimpleTest/DataRecorder.h
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
    Planning/LLMPlan/FomPlanStrategy.cc
    Planning/LLMPlan/FomPlanStrategy.h
    Planning/LLMPlan/CustomPlanStrategy.cc
    Planning/LLMPlan/CustomPlanStrategy.h
    Planning/LLMPlan/SquarePlanStrategy.cc
    Planning/LLMPlan/SquarePlanStrategy.h
    Settings/LLMSettings.cc
    Settings/LLMSettings.h
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Common
    ${CMAKE_CURRENT_SOURCE_DIR}/Common/SingletonTest
    ${CMAKE_CURRENT_SOURCE_DIR}/Archive
    ${CMAKE_CURRENT_SOURCE_DIR}/Settings
    ${CMAKE_CURRENT_SOURCE_DIR}/Survey/SimpleTest
    ${CMAKE_CURRENT_SOURCE_DIR}/Attitude/Yaw
    ${CMAKE_CURRENT_SOURCE_DIR}/MagMap
    ${CMAKE_CURRENT_SOURCE_DIR}/Planning/LLMPlan
    ${CMAKE_CURRENT_SOURCE_DIR}/Planning/Sketch
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
set_source_files_properties(Images/MaglabIcon.svg PROPERTIES QT_RESOURCE_ALIAS MaglabIcon.svg)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_collection_page
    PREFIX "/maglab"
    FILES Survey/SimpleTest/CollectionPage.qml Survey/SimpleTest/MediaCapturePage.qml
          Survey/SimpleTest/MagMapGalleryPage.qml MagLabPanel.qml Images/MaglabIcon.svg
          DiagnosticPage.qml ExecutionObserver.qml
)

# Package under distinct names: the LLMSettings backend is a QML type in QGC.
set_source_files_properties(Settings/qml/LLMSettings.qml PROPERTIES QT_RESOURCE_ALIAS LLMSettingsPage.qml)
set_source_files_properties(Settings/qml/TaskNameTimeSettings.qml PROPERTIES QT_RESOURCE_ALIAS TaskNameTimeSettings.qml)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_settings_pages
    PREFIX "/maglab"
    FILES Settings/qml/LLMSettings.qml Settings/qml/TaskNameTimeSettings.qml
)

set_source_files_properties(Settings/LLM.SettingsGroup.json PROPERTIES QT_RESOURCE_ALIAS LLM.SettingsGroup.json)
qt_add_resources(${CMAKE_PROJECT_NAME} maglab_settings_resources
    PREFIX "/json"
    FILES Settings/LLM.SettingsGroup.json
)
