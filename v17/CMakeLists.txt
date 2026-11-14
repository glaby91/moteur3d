cmake_minimum_required(VERSION 3.24)
project(CSE_WARP_V17 VERSION 17.0 LANGUAGES CXX)
# GitHub Actions run number becomes the actual build number in both EXEs.
# Local developer builds are explicitly numbered 0.
set(V17_BUILD_NUMBER 0 CACHE STRING "CI run number for Windows version resource")
if(NOT V17_BUILD_NUMBER MATCHES "^[0-9]+$")
    message(FATAL_ERROR "V17_BUILD_NUMBER must be an unsigned integer")
endif()
if(V17_BUILD_NUMBER GREATER 65535)
    message(FATAL_ERROR "Windows file version build component must fit 16 bits")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

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
        "${CMAKE_CURRENT_SOURCE_DIR}/radio_audio.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/v17_app.rc")
    add_dependencies(moteur3d_v17 v17_icon_generator)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/v17_app.rc"
        PROPERTIES OBJECT_DEPENDS "${V17_ICON}")
else()
    qt_add_executable(moteur3d_v17
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_qt.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/radio_audio.cpp")
endif()
target_include_directories(moteur3d_v17 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(moteur3d_v17 PRIVATE moteur3d_v16_core Qt6::Widgets Qt6::Concurrent)
if(WIN32)
    # About dialog queries the DXGI primary adapter name without changing device selection.
    target_link_libraries(moteur3d_v17 PRIVATE dxgi xaudio2)
endif()
target_compile_definitions(moteur3d_v17 PRIVATE CSE_WARP_BUILD_NUMBER=${V17_BUILD_NUMBER})
if(MSVC)
    target_compile_options(moteur3d_v17 PRIVATE /O2 /EHsc /utf-8)
endif()
# Repeatable original 30s PCM diagnostic loop; no copyright-sensitive downloads
# and no external audio processing tool required by the Windows build.
if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(V17_RADIO_TEST_WAV "${CMAKE_CURRENT_BINARY_DIR}/radio-assets/pripyat_radio.wav")
    add_custom_command(
        OUTPUT "${V17_RADIO_TEST_WAV}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/radio-assets"
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../assets/audio/pripyat_radio/generate_test_wav.py"
            "${V17_RADIO_TEST_WAV}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/audio/pripyat_radio/generate_test_wav.py"
        VERBATIM)
    add_custom_target(v17_radio_test_asset DEPENDS "${V17_RADIO_TEST_WAV}")
    add_dependencies(moteur3d_v17 v17_radio_test_asset)
endif()
add_custom_command(TARGET moteur3d_v17 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:moteur3d_v17>/audio"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../assets/audio/pripyat_radio/speaker_layout.json"
      "$<TARGET_FILE_DIR:moteur3d_v17>/audio/speaker_layout.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${V17_RADIO_TEST_WAV}"
      "$<TARGET_FILE_DIR:moteur3d_v17>/audio/pripyat_radio.wav"
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
if(WIN32)
    add_executable(moteur3d_v17_benchmark
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark_v17.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/v17_benchmark_runner.rc")
    add_dependencies(moteur3d_v17_benchmark v17_icon_generator)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/v17_benchmark_runner.rc"
        PROPERTIES OBJECT_DEPENDS "${V17_BENCH_ICON}")
else()
    add_executable(moteur3d_v17_benchmark
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark_v17.cpp")
endif()
target_include_directories(moteur3d_v17_benchmark PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(moteur3d_v17_benchmark PRIVATE
    moteur3d_v16_core Qt6::Core Qt6::Gui)
target_compile_definitions(moteur3d_v17_benchmark PRIVATE CSE_WARP_BUILD_NUMBER=${V17_BUILD_NUMBER})

# Independent one-route hardware D3D11 profiler (no full benchmark campaign).
# All telemetry is produced in CSV/JSON, with three quality-check PNG frames.
if(WIN32)
    add_executable(moteur3d_v17_gpu_trajectory
        "tools/benchmark_gpu_trajectory.cpp")
    target_include_directories(moteur3d_v17_gpu_trajectory PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(moteur3d_v17_gpu_trajectory PRIVATE
        moteur3d_v16_core Qt6::Core Qt6::Gui psapi pdh dxgi)
    target_compile_definitions(moteur3d_v17_gpu_trajectory PRIVATE
        CSE_WARP_BUILD_NUMBER=${V17_BUILD_NUMBER})
    if(MSVC)
        target_compile_options(moteur3d_v17_gpu_trajectory PRIVATE /O2 /EHsc /utf-8)
    endif()
endif()

# Offline multi-hour GI bake: CPU ray transport, disk table and benchmark.
add_executable(moteur3d_v17_gibake "tools/gi_bake_cli.cpp")
target_include_directories(moteur3d_v17_gibake PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(moteur3d_v17_gibake PRIVATE moteur3d_v16_core Qt6::Core)
target_compile_definitions(moteur3d_v17_gibake PRIVATE CSE_WARP_BUILD_NUMBER=${V17_BUILD_NUMBER})
if(MSVC)
    target_compile_options(moteur3d_v17_gibake PRIVATE /O2 /EHsc /utf-8)
endif()
if(WIN32)
    qt_add_executable(moteur3d_v17_benchui WIN32
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark_v17_ui.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/v17_benchmark.rc")
    add_dependencies(moteur3d_v17_benchui v17_icon_generator)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/v17_benchmark.rc"
        PROPERTIES OBJECT_DEPENDS "${V17_BENCH_ICON}")
else()
    qt_add_executable(moteur3d_v17_benchui
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark_v17_ui.cpp")
endif()
# The complete benchmark is a separate tool in the SAME Windows build:
# no iterative recompilation is required to change rounds/cameras/buffers.
add_executable(moteur3d_v17_campaign
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark_v17_campaign.cpp")
target_include_directories(moteur3d_v17_campaign PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(moteur3d_v17_campaign PRIVATE
    moteur3d_v16_core Qt6::Core Qt6::Gui)
target_compile_definitions(moteur3d_v17_campaign PRIVATE
    CSE_WARP_BUILD_NUMBER=${V17_BUILD_NUMBER})
if(MSVC)
    target_compile_options(moteur3d_v17_campaign PRIVATE /O2 /EHsc /utf-8)
endif()

# All auxiliary Windows executables must report exactly the same build
# number as the front-end and benchmark icons.
if(WIN32)
    foreach(aux IN ITEMS moteur3d_v17_gibake moteur3d_v17_campaign moteur3d_v17_gpu_trajectory)
        set(V17_AUX_NAME "${aux}")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/v17_auxiliary.rc.in"
                       "${CMAKE_CURRENT_BINARY_DIR}/${aux}.rc" @ONLY)
        target_sources(${aux} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${aux}.rc")
    endforeach()
endif()

target_link_libraries(moteur3d_v17_benchui PRIVATE Qt6::Widgets)
target_compile_definitions(moteur3d_v17_benchui PRIVATE CSE_WARP_BUILD_NUMBER=${V17_BUILD_NUMBER})
if(WIN32)
    add_custom_command(TARGET moteur3d_v17_benchui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${V17_BENCH_ICON}"
            "$<TARGET_FILE_DIR:moteur3d_v17_benchui>/cse_warp_v17_bench.ico"
        VERBATIM)
endif()
if(MSVC)
    target_compile_options(moteur3d_v17_benchmark PRIVATE /O2 /EHsc /utf-8)
    target_compile_options(moteur3d_v17_benchui PRIVATE /O2 /EHsc /utf-8)
endif()
install(TARGETS moteur3d_v17 moteur3d_v17_benchmark moteur3d_v17_benchui moteur3d_v17_campaign
    RUNTIME DESTINATION .)

if(WIN32)
    # Dedicated generic geometry/material loader validation. Do not use the
    # Pavillon 01 fixture, which hardcodes its original 15 foliage canopies.
    add_executable(v17_benchmark_scene_load_smoke "tests/benchmark_scene_load_smoke.cpp")
    target_include_directories(v17_benchmark_scene_load_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_benchmark_scene_load_smoke PRIVATE
        moteur3d_v16_core Qt6::Core)
    if(MSVC)
        target_compile_options(v17_benchmark_scene_load_smoke PRIVATE /O2 /EHsc /utf-8)
    endif()
    # Deterministic PBR multithreading and heterogeneous Compute mip smoke.
    # Runs in WARP with textures sized 512px (serial vs parallel) and 256px
    # (historical failure: no 512px mip for the Compute atlas).
    # Real-scene A/B: native primary-ray counts, CPU/Compute donor parity
    # and four diagnostic PNGs from the pool camera at 19:00.
    add_executable(v17_primary_quarter_pripyat_smoke
        "tests/primary_quarter_pripyat_smoke.cpp")
    target_include_directories(v17_primary_quarter_pripyat_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_primary_quarter_pripyat_smoke PRIVATE
        moteur3d_v16_core Qt6::Core Qt6::Gui)
    if(MSVC)
        target_compile_options(v17_primary_quarter_pripyat_smoke PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_executable(v17_pbr_compute_smoke "tests/pbr_compute_smoke.cpp")
    target_include_directories(v17_pbr_compute_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_pbr_compute_smoke PRIVATE
        moteur3d_v16_core Qt6::Core)
    if(MSVC)
        target_compile_options(v17_pbr_compute_smoke PRIVATE /O2 /EHsc /utf-8)
    endif()
    # GI transport bake: deterministic ray tracing, 25-hour format and cache checks.
    add_executable(v17_gi_bake_smoke "tests/gi_bake_smoke.cpp")
    target_include_directories(v17_gi_bake_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_gi_bake_smoke PRIVATE moteur3d_v16_core)
    if(MSVC)
        target_compile_options(v17_gi_bake_smoke PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME v17_gi_table_precompute_and_checksum
        COMMAND v17_gi_bake_smoke "${CMAKE_CURRENT_BINARY_DIR}/v17-gi-smoke.m3dgi")
    set_tests_properties(v17_gi_table_precompute_and_checksum PROPERTIES TIMEOUT 45)
    # Pure CPU walker regression, no Qt or DXR requirement.
    add_executable(v17_walk_controller_smoke "tests/walk_controller_smoke.cpp")
    target_include_directories(v17_walk_controller_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_walk_controller_smoke PRIVATE moteur3d_v16_core)
    if(MSVC)
        target_compile_options(v17_walk_controller_smoke PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME v17_walk_stairs_collision COMMAND v17_walk_controller_smoke)
    set_tests_properties(v17_walk_stairs_collision PROPERTIES TIMEOUT 30)
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
