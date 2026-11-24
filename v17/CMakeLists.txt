cmake_minimum_required(VERSION 3.24)
project(CSE_WARP_V17 VERSION 17.0 LANGUAGES CXX)
# Product, module and scene revisions are independent version axes.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.json" V17_VERSION_JSON)
string(JSON V17_ENGINE_MAJOR GET "${V17_VERSION_JSON}" engine major)
string(JSON V17_BUILD_NUMBER GET "${V17_VERSION_JSON}" engine build)
string(JSON V17_MODULE_MANIFEST_RELATIVE GET "${V17_VERSION_JSON}" renderModules)
get_filename_component(V17_MODULE_MANIFEST
    "${CMAKE_CURRENT_SOURCE_DIR}/${V17_MODULE_MANIFEST_RELATIVE}" ABSOLUTE)
get_filename_component(V17_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
include("${V17_REPOSITORY_ROOT}/cmake/RenderModules.cmake")
m3d_validate_render_modules("${V17_MODULE_MANIFEST}" "${V17_REPOSITORY_ROOT}")
m3d_render_module_revision("${V17_MODULE_MANIFEST}" cpu_shading V17_COMPONENT_PBR)
m3d_render_module_revision("${V17_MODULE_MANIFEST}" cpu_lighting V17_COMPONENT_CPU_LIGHTING)
m3d_render_module_revision("${V17_MODULE_MANIFEST}" d3d11_raster V17_COMPONENT_DIRECTX11)
m3d_render_module_revision("${V17_MODULE_MANIFEST}" vulkan_raster V17_COMPONENT_VULKAN)
m3d_render_module_revision("${V17_MODULE_MANIFEST}" lumatrace_bvh4 V17_COMPONENT_LUMATRACE)
m3d_render_module_revision("${V17_MODULE_MANIFEST}" lumatrace_gpu_compute V17_COMPONENT_LUMATRACE_GPU)
if(V17_BUILD_NUMBER LESS 10)
    set(V17_BUILD_PADDED "00${V17_BUILD_NUMBER}")
elseif(V17_BUILD_NUMBER LESS 100)
    set(V17_BUILD_PADDED "0${V17_BUILD_NUMBER}")
else()
    set(V17_BUILD_PADDED "${V17_BUILD_NUMBER}")
endif()
set(V17_PRODUCT_VERSION "${V17_ENGINE_MAJOR}.${V17_BUILD_PADDED}")
set(V17_CI_BUILD_NUMBER "LOCAL" CACHE STRING "Official GitHub Actions run number")
set(V17_GIT_SHA "unknown" CACHE STRING "Git commit included in this build")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/v17_build_info.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/v17_build_info.h" @ONLY)
if(V17_BUILD_NUMBER GREATER 65535)
    message(FATAL_ERROR "Windows file version build component must fit 16 bits")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Package the latest separately versioned V17 scene when CI supplies it.
# Offline/local builds keep the checked-in scene as a deterministic fallback.
set(V17_SCENE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/benchmark-civic"
    CACHE PATH "Scene directory copied into the V17 application package")
if(NOT EXISTS "${V17_SCENE_SOURCE_DIR}/scene.json")
    message(FATAL_ERROR "V17 scene package is missing scene.json: ${V17_SCENE_SOURCE_DIR}")
endif()
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Concurrent Network Multimedia)

enable_testing()
# CSE WARP Physics / Weather: deterministic fair-weather cirrus smoke test.
add_executable(v17_weather_clouds_smoke "tests/weather_clouds_smoke.cpp")
add_test(NAME v17_weather_cirrus_deterministic COMMAND v17_weather_clouds_smoke)
add_executable(v17_weather_solar_cycle_smoke "tests/weather_solar_cycle_smoke.cpp")
add_test(NAME v17_weather_solar_24h_continuity COMMAND v17_weather_solar_cycle_smoke)
add_executable(v17_weather_sky_compositor_smoke "tests/weather_sky_compositor_smoke.cpp")
target_include_directories(v17_weather_sky_compositor_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(v17_weather_sky_compositor_smoke PRIVATE Qt6::Gui)
add_test(NAME v17_weather_independent_30hz_sky COMMAND v17_weather_sky_compositor_smoke)
add_executable(v17_weather_astronomy_smoke "tests/weather_astronomy_smoke.cpp")
add_test(NAME v17_weather_astronomy_day_night COMMAND v17_weather_astronomy_smoke)
# V17.443: anomaly catalogue and historically verified camera positions.
add_executable(v17_anomaly_catalog_smoke "tests/anomaly_catalog_smoke.cpp")
add_test(NAME v17_anomaly_catalog_registry COMMAND v17_anomaly_catalog_smoke)
add_executable(v17_pool_ladder_smoke "tests/pool_ladder_smoke.cpp")
add_test(NAME v17_pool_ladder_bidirectional COMMAND v17_pool_ladder_smoke)
# Frame-pacing regression is GPU-free: measurable present intervals and scene safety.
add_executable(v17_frame_pacing_smoke "tests/frame_pacing_smoke.cpp")
add_test(NAME v17_frame_pacing_statistical_regression COMMAND v17_frame_pacing_smoke)
# GUI-independent regression for exact, portable architectural camera poses.
add_executable(v17_camera_diagnostic_smoke
    "tests/camera_diagnostic_smoke.cpp")
target_include_directories(v17_camera_diagnostic_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(v17_camera_diagnostic_smoke PRIVATE Qt6::Core)
add_test(NAME v17_camera_diagnostic_roundtrip COMMAND v17_camera_diagnostic_smoke)
# Pure CPU test for RMB-held scoped FOV, smooth release and aim sensitivity.
add_executable(v17_scope_zoom_smoke "tests/scope_zoom_smoke.cpp")
add_test(NAME v17_scope_zoom_hold_release COMMAND v17_scope_zoom_smoke)

# No GPU or Windows runtime required to verify exact 2x2 sample budget.
add_executable(v17_primary_quarter_smoke "tests/primary_quarter_smoke.cpp")
add_test(NAME v17_primary_quarter_exact_budget COMMAND v17_primary_quarter_smoke)
# Reuse the proven V16 CPU, Compute and raster engines, plus their tests.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../v16" "${CMAKE_CURRENT_BINARY_DIR}/v16")

# The Windows ICO is compiled into the EXE so Explorer/taskbar recognize V17.
# Generate all 16..256px sizes from the local brand artwork; no external download.
if(WIN32)
    set(V17_ICON "${CMAKE_CURRENT_BINARY_DIR}/cse_warp_v17.ico")
    set(V17_BENCH_ICON "${CMAKE_CURRENT_BINARY_DIR}/cse_warp_v17_bench.ico")
    add_custom_command(OUTPUT "${V17_ICON}"
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_windows_icon.ps1"
            -OutputPath "${V17_ICON}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_windows_icon.ps1"
        COMMENT "Generate CSE WARP V17 Windows icon (16..256px)"
        VERBATIM)
    add_custom_command(OUTPUT "${V17_BENCH_ICON}"
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_windows_icon.ps1"
            -OutputPath "${V17_BENCH_ICON}" -Benchmark
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_windows_icon.ps1"
        COMMENT "Generate separate CSE WARP V17.3 Benchmark Windows icon"
        VERBATIM)
    add_custom_target(v17_icon_generator DEPENDS "${V17_ICON}" "${V17_BENCH_ICON}")
    file(TO_CMAKE_PATH "${V17_ICON}" V17_ICON_RC_PATH)
    file(TO_CMAKE_PATH "${V17_BENCH_ICON}" V17_BENCH_ICON_RC_PATH)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/v17_app.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/v17_app.rc" @ONLY)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/v17_benchmark.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/v17_benchmark.rc" @ONLY)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/v17_benchmark_runner.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/v17_benchmark_runner.rc" @ONLY)
    qt_add_executable(moteur3d_v17 WIN32
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_qt.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_cpu.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_compute.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_d3d11.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_lumatrace.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_vulkan.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/radio_audio.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/v17_app.rc")
    add_dependencies(moteur3d_v17 v17_icon_generator)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/v17_app.rc"
        PROPERTIES OBJECT_DEPENDS "${V17_ICON}")
else()
    qt_add_executable(moteur3d_v17
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_qt.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_cpu.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_compute.cpp"
       