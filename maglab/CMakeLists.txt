# Retain upstream firmware plugins, resources and features.
# Android 3D is experimental and must not change upstream builds.
set(CUSTOM_DEFINITIONS MAGLAB_ANDROID_3D_EXPERIMENTAL CACHE INTERNAL "Maglab experimental features" FORCE)

if(ANDROID)
    # Stage a private template so branding never modifies the upstream Android resources.
    set(_maglab_android_dir "${CMAKE_BINARY_DIR}/maglab/android")
    file(COPY "${CMAKE_SOURCE_DIR}/android/" DESTINATION "${_maglab_android_dir}")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/android/" DESTINATION "${_maglab_android_dir}")
    set(QGC_ANDROID_PACKAGE_SOURCE_DIR "${_maglab_android_dir}" CACHE PATH "Maglab Android template" FORCE)
endif()
