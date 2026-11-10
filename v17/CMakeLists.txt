cmake_minimum_required(VERSION 3.24)
project(CSE_WARP_V17 VERSION 17.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

enable_testing()
# Reuse the proven V16 CPU, Compute and raster engines, plus their tests.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../v16" "${CMAKE_CURRENT_BINARY_DIR}/v16")

# The Windows ICO is compiled into the EXE so Explorer/taskbar recognize V17.
# Generate all 16..256px sizes from the local brand artwork; no external download.
if(WIN32)
    set(V17_ICON "${CMAKE_CURRENT_BINARY_DIR}/cse_warp_v17.ico")
    add_custom_command(OUTPUT "${V17_ICON}"
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_windows_icon.ps1"
            -OutputPath "${V17_ICON}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_windows_icon.ps1"
        COMMENT "Generate CSE WARP V17 Windows icon (16..256px)"
        VERBATIM)
    add_custom_target(v17_icon_generator DEPENDS "${V17_ICON}")
    file(TO_CMAKE_PATH "${V17_ICON}" V17_ICON_RC_PATH)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/v17_app.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/v17_app.rc" @ONLY)
    qt_add_executable(moteur3d_v17 WIN32
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_qt.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/v17_app.rc")
    add_dependencies(moteur3d_v17 v17_icon_generator)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/v17_app.rc"
        PROPERTIES OBJECT_DEPENDS "${V17_ICON}")
else()
    qt_add_executable(moteur3d_v17
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_qt.cpp")
endif()
target_include_directories(moteur3d_v17 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(moteur3d_v17 PRIVATE moteur3d_v16_core Qt6::Widgets Qt6::Concurrent)
if(MSVC)
    target_compile_options(moteur3d_v17 PRIVATE /O2 /EHsc /utf-8)
endif()
add_custom_command(TARGET moteur3d_v17 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${V17_ICON}"
      "$<TARGET_FILE_DIR:moteur3d_v17>/cse_warp_v17.ico"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:moteur3d_v16_core>"
      "$<TARGET_FILE_DIR:moteur3d_v17>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01"
      "$<TARGET_FILE_DIR:moteur3d_v17>/pavillon01"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/material-lab"
      "$<TARGET_FILE_DIR:moteur3d_v17>/material-lab"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pripyat-photo2scene-proof"
      "$<TARGET_FILE_DIR:moteur3d_v17>/pripyat-photo2scene-proof"
    # Generated, independently authored V17 benchmark with three load profiles.
    # It is generated in GitHub Actions before CMake configuration.
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/benchmark-civic"
      "$<TARGET_FILE_DIR:moteur3d_v17>/benchmark-civic"
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:moteur3d_v17>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14/src/shaders/m3d_raytrace_compute.hlsl"
      "$<TARGET_FILE_DIR:moteur3d_v17>/shaders/m3d_raytrace_compute.hlsl"
    VERBATIM)
# Standalone V17 benchmark + GUI. The measured engine is the same
# shared V16 DLL and native D3D11 raster path as the V17 exploration view.
add_executable(moteur3d_v17_benchmark
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark_v17.cpp")
target_include_directories(moteur3d_v17_benchmark PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(moteur3d_v17_benchmark PRIVATE
    moteur3d_v16_core Qt6::Core Qt6::Gui)
qt_add_executable(moteur3d_v17_benchui WIN32
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark_v17_ui.cpp")
target_link_libraries(moteur3d_v17_benchui PRIVATE Qt6::Widgets)
if(MSVC)
    target_compile_options(moteur3d_v17_benchmark PRIVATE /O2 /EHsc /utf-8)
    target_compile_options(moteur3d_v17_benchui PRIVATE /O2 /EHsc /utf-8)
endif()
install(TARGETS moteur3d_v17 moteur3d_v17_benchmark moteur3d_v17_benchui
    RUNTIME DESTINATION .)

if(WIN32)
    add_executable(v17_raster_warp_smoke "tests/raster_warp_smoke.cpp")
    target_include_directories(v17_raster_warp_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_raster_warp_smoke PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
    if(MSVC)
        target_compile_options(v17_raster_warp_smoke PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME v17_d3d11_raster_warp
        COMMAND v17_raster_warp_smoke
            "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01/scene.json"
            "${CMAKE_CURRENT_BINARY_DIR}/v17-raster-warp.png")
    set_tests_properties(v17_d3d11_raster_warp PROPERTIES
        TIMEOUT 180 ENVIRONMENT "M3D_V16_FORCE_WARP=1")
endif()
