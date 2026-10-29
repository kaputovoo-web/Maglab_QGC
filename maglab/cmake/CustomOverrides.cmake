# Enable with -DQGC_CUSTOM_DIR=maglab; leave upstream builds unchanged.
set(QGC_APP_NAME "Maglab" CACHE STRING "Application name" FORCE)
set(QGC_MACOS_ICON_PATH "${CMAKE_CURRENT_LIST_DIR}/../res/maglab.icns" CACHE FILEPATH "Maglab macOS application icon" FORCE)
# Local testing only; choose the production application ID before release.
set(QGC_ANDROID_PACKAGE_NAME "org.maglab.groundcontrol.test" CACHE STRING "Android package identifier" FORCE)
