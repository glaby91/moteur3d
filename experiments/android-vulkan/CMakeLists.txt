cmake_minimum_required(VERSION 3.24)
project(CSE_WARP_ANDROID_PORT LANGUAGES CXX)
if(NOT ANDROID)
    message(FATAL_ERROR "Use Qt for Android qt-cmake and the pinned NDK")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(M3D_REQUIRE_VULKAN ON CACHE BOOL "No Vulkan stub in Android" FORCE)
set(CSE_ANDROID_PACKAGE_DIR "" CACHE PATH "Staged Android package template + verified assets")
set(CSE_ANDROID_BUILD_ID "local" CACHE STRING "Source SHA")
if(NOT EXISTS "${CSE_ANDROID_PACKAGE_DIR}/assets/cse/index.txt")
    message(FATAL_ERROR "Run stage_assets.py before configuring")
endif()
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
# Android deployment paths contain TARGET_FILE generator expressions.
qt_policy(SET QTP0002 NEW)
find_package(Vulkan 1.3.204 REQUIRED)
get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory("${REPO}/v16" "${CMAKE_CURRENT_BINARY_DIR}/v16")
qt_add_executable(cse_android_probe MANUAL_FINALIZATION main.cpp)
target_link_libraries(cse_android_probe PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui Vulkan::Vulkan)
target_compile_options(cse_android_probe PRIVATE -O2)
target_compile_definitions(cse_android_probe PRIVATE CSE_ANDROID_BUILD_ID="${CSE_ANDROID_BUILD_ID}")
if(ANDROID_ABI STREQUAL "x86_64")
    target_sources(cse_android_probe PRIVATE walk_regression.cpp)
    target_compile_definitions(cse_android_probe PRIVATE CSE_ANDROID_EMULATOR_TEST=1)
endif()
set_target_properties(cse_android_probe PROPERTIES
    QT_ANDROID_PACKAGE_NAME "org.csewarp.androidprobe"
    QT_ANDROID_APP_NAME "CSE WARP Android Port Probe"
    QT_ANDROID_VERSION_NAME "17.031"
    QT_ANDROID_VERSION_CODE 1
    QT_ANDROID_SDK_BUILD_TOOLS_REVISION "36.0.0"
    QT_ANDROID_COMPILE_SDK_VERSION "36"
    QT_ANDROID_MIN_SDK_VERSION 36
    QT_ANDROID_TARGET_SDK_VERSION 36
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CSE_ANDROID_PACKAGE_DIR}"
    QT_ANDROID_EXTRA_LIBS "$<TARGET_FILE:moteur3d_v16_core>")
qt_finalize_executable(cse_android_probe)
