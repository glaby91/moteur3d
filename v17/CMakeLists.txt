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
m3d_render_module_revision("${V17_MODULE_MANIFEST}" lumatrace_cpu V17_COMPONENT_LUMATRACE)
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

# Exact CPU-only checks for the reusable static indexed-geometry preparation.
add_executable(v17_static_geometry_prepare_smoke
    "tests/static_geometry_prepare_smoke.cpp")
add_test(NAME v17_static_geometry_prepare_attributes
    COMMAND v17_static_geometry_prepare_smoke)

add_executable(v17_zmaterial_smoke tests/zmaterial_smoke.cpp)
target_link_libraries(v17_zmaterial_smoke PRIVATE Qt6::Core)
add_custom_command(TARGET v17_zmaterial_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:v17_zmaterial_smoke>/materials"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/materials/zmaterial8192.json" "$<TARGET_FILE_DIR:v17_zmaterial_smoke>/materials/zmaterial8192.json")
add_test(NAME v17_zmaterial_runtime COMMAND v17_zmaterial_smoke)
add_executable(v17_atmosphere_smoke tests/atmosphere_smoke.cpp)
add_test(NAME v17_atmosphere_math COMMAND v17_atmosphere_smoke)
add_executable(v17_dx11_emission_smoke tests/dx11_emission_smoke.cpp)
add_test(NAME v17_dx11_emission_five_checks COMMAND v17_dx11_emission_smoke)
add_executable(v17_dx11_r001_smoke tests/dx11_r001_smoke.cpp)
add_test(NAME v17_dx11_r001_five_checks COMMAND v17_dx11_r001_smoke)

# No GPU or Windows runtime required to verify exact 2x2 sample budget.
add_executable(v17_primary_quarter_smoke "tests/primary_quarter_smoke.cpp")
add_test(NAME v17_primary_quarter_exact_budget COMMAND v17_primary_quarter_smoke)
# Reuse the proven V16 CPU, Compute and raster engines, plus their tests.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../v16" "${CMAKE_CURRENT_BINARY_DIR}/v16")

# CSP001 offline compiler. Uses the same scene builder as the runtime.
add_executable(cse_scene_precompiler tools/cse_scene_precompiler.cpp)
target_link_libraries(cse_scene_precompiler PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
add_executable(v17_csp001_smoke tests/csp001_smoke.cpp)
target_link_libraries(v17_csp001_smoke PRIVATE moteur3d_v16_core Qt6::Core)
add_test(NAME v17_csp001_roundtrip_visibility COMMAND v17_csp001_smoke)
add_executable(v17_zreflect_catalog_smoke tests/zreflect_catalog_smoke.cpp)
target_include_directories(v17_zreflect_catalog_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
add_test(NAME v17_zreflect_catalog_filter COMMAND v17_zreflect_catalog_smoke)
add_executable(v17_zreflect_index_smoke tests/zreflect_index_smoke.cpp)
target_include_directories(v17_zreflect_index_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
add_test(NAME v17_zreflect_spatial_60000 COMMAND v17_zreflect_index_smoke)
if(WIN32)
    add_executable(v17_atmosphere_warp_ab tests/atmosphere_warp_ab.cpp)
    target_link_libraries(v17_atmosphere_warp_ab PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
    add_executable(v17_dx11_emission_warp_ab tests/dx11_emission_warp_ab.cpp)
    target_link_libraries(v17_dx11_emission_warp_ab PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
    add_executable(v17_dx11_r001_warp_ab tests/dx11_r001_warp_ab.cpp)
    target_link_libraries(v17_dx11_r001_warp_ab PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
    add_executable(v17_zreflect_warp_ab tests/zreflect_warp_ab.cpp)
    target_link_libraries(v17_zreflect_warp_ab PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
endif()

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
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_d3d11.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_lumatrace.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_render_vulkan.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/radio_audio.cpp")
endif()
target_include_directories(moteur3d_v17 PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(moteur3d_v17 PRIVATE moteur3d_v16_core Qt6::Widgets Qt6::Concurrent Qt6::Network Qt6::Multimedia)
if(WIN32)
    # About dialog queries the DXGI primary adapter name without changing device selection.
    target_link_libraries(moteur3d_v17 PRIVATE dxgi xaudio2)
endif()
target_compile_definitions(moteur3d_v17 PRIVATE CSE_WARP_BUILD_NUMBER=${V17_BUILD_NUMBER})
if(MSVC)
    target_compile_options(moteur3d_v17 PRIVATE /O2 /EHsc /utf-8)
endif()
# Vulkan 1.3.204 minimum Raster backend: compile native GLSL shaders to SPIR-V.
# Pipeline assembly is done once per worker thread, not in the per-frame path.
if(WIN32)
    find_package(Vulkan 1.3.204 QUIET)
    if(Vulkan_FOUND)
        find_program(V17_GLSLANG_VALIDATOR NAMES glslangValidator glslangValidator.exe
            HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32")
        if(NOT V17_GLSLANG_VALIDATOR)
            message(WARNING "Vulkan SDK present but glslangValidator not found: Vulkan preview requires SPIR-V shaders")
        else()
            set(V17_VULKAN_SPIRV)
            foreach(stage IN ITEMS vert frag)
                set(spv "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders/raster.${stage}.spv")
                add_custom_command(OUTPUT "${spv}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                        "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders"
                    COMMAND "${V17_GLSLANG_VALIDATOR}"
                        -V --target-env vulkan1.3
                        "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/raster.${stage}"
                        -o "${spv}"
                    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/raster.${stage}"
                    VERBATIM)
                list(APPEND V17_VULKAN_SPIRV "${spv}")
            endforeach()
            # Fullscreen atmospheric sky: V17 DX11 model ported to native GLSL.
            set(sky_spv)
            foreach(stage IN ITEMS vert frag)
                set(skyOut "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders/sky.${stage}.spv")
                add_custom_command(OUTPUT "${skyOut}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                        "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders"
                    COMMAND "${V17_GLSLANG_VALIDATOR}"
                        -V --target-env vulkan1.3
                        "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/sky.${stage}"
                        -o "${skyOut}"
                    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/sky.${stage}"
                    VERBATIM)
                list(APPEND V17_VULKAN_SPIRV "${skyOut}")
            endforeach()
            # Z Atmosphere depth-input fullscreen pass (single / MSAA).
            foreach(mode IN ITEMS single ms)
                if(mode STREQUAL "ms")
                    set(fogSpv "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders/atmosphere_ms.frag.spv")
                    set(fogDefine "-DZATM_MSAA=1")
                else()
                    set(fogSpv "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders/atmosphere.frag.spv")
                    set(fogDefine "")
                endif()
                add_custom_command(OUTPUT "${fogSpv}"
                    COMMAND "${V17_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 ${fogDefine}
                    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/atmosphere.frag" -o "${fogSpv}"
                    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/atmosphere.frag" VERBATIM)
                list(APPEND V17_VULKAN_SPIRV "${fogSpv}")
            endforeach()
            # Two fragment variants from the same source. Optimized is the
            # runtime default; set CSE_WARP_VK_FAST_FRAGMENT=0 for reference A/B.
            set(fast_frag_spv "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders/raster_fast.frag.spv")
            add_custom_command(OUTPUT "${fast_frag_spv}"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders"
                COMMAND "${V17_GLSLANG_VALIDATOR}"
                    -V --target-env vulkan1.3 -DCSE_WARP_VK_FAST_FRAGMENT=1
                    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/raster.frag"
                    -o "${fast_frag_spv}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/raster.frag"
                VERBATIM)
            list(APPEND V17_VULKAN_SPIRV "${fast_frag_spv}")
            set(box_spv "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders/raster_box.vert.spv")
            add_custom_command(OUTPUT "${box_spv}"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders"
                COMMAND "${V17_GLSLANG_VALIDATOR}"
                    -V --target-env vulkan1.3
                    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/raster_box.vert"
                    -o "${box_spv}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/raster_box.vert"
                VERBATIM)
            list(APPEND V17_VULKAN_SPIRV "${box_spv}")
            set(foliage_spv "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders/raster_foliage.vert.spv")
            add_custom_command(OUTPUT "${foliage_spv}"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${CMAKE_CURRENT_BINARY_DIR}/vulkan-shaders"
                COMMAND "${V17_GLSLANG_VALIDATOR}"
                    -V --target-env vulkan1.3
                    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/raster_foliage.vert"
                    -o "${foliage_spv}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/raster_foliage.vert"
                VERBATIM)
            list(APPEND V17_VULKAN_SPIRV "${foliage_spv}")
            add_custom_target(v17_vulkan_spirv DEPENDS ${V17_VULKAN_SPIRV})
            add_dependencies(moteur3d_v17 v17_vulkan_spirv)
            add_custom_command(TARGET moteur3d_v17 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:moteur3d_v17>/vulkan"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${V17_VULKAN_SPIRV}
                    "$<TARGET_FILE_DIR:moteur3d_v17>/vulkan"
                VERBATIM)
        endif()
    endif()
endif()

# Dedicated Vulkan regression tests. GPU-free mesh parity always runs.
add_executable(v17_vulkan_dirty_policy_smoke "vulkan/tests/dirty_policy_smoke.cpp")
add_test(NAME v17_vulkan_incremental_dirty_policy COMMAND v17_vulkan_dirty_policy_smoke)
add_executable(v17_vulkan_mipmap_smoke "vulkan/tests/mipmap_smoke.cpp")
target_include_directories(v17_vulkan_mipmap_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan")
add_test(NAME v17_vulkan_mipmap_policy COMMAND v17_vulkan_mipmap_smoke)

add_executable(v17_vulkan_readback_smoke "vulkan/tests/readback_smoke.cpp")
target_include_directories(v17_vulkan_readback_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan")
add_test(NAME v17_vulkan_rgba_bgra_argb32_parity COMMAND v17_vulkan_readback_smoke)
add_executable(v17_vulkan_mesh_smoke "vulkan/tests/mesh_smoke.cpp")
target_include_directories(v17_vulkan_mesh_smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan")
# CPU-only mesh regression: deliberately do not load the full V16 GPU-enabled
# shared DLL (and its DirectX/Qt/Vulkan loader dependencies) on CI runners.
# A fixture-local groundMaterial() is supplied by mesh_smoke.cpp.
# The actual shared engine DLL is still built/packaged and used by GPU tests.
add_test(NAME v17_vulkan_mesh_tessellation COMMAND v17_vulkan_mesh_smoke)
if(WIN32 AND Vulkan_FOUND)
    add_executable(v17_vulkan_version_smoke "vulkan/tests/version_smoke.cpp")
    target_include_directories(v17_vulkan_version_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/vulkan")
    target_link_libraries(v17_vulkan_version_smoke PRIVATE Vulkan::Vulkan)
    add_test(NAME v17_vulkan_1_3_204_policy COMMAND v17_vulkan_version_smoke)
    add_executable(v17_vulkan_gpu_smoke "vulkan/tests/vulkan_gpu_smoke.cpp")
    target_include_directories(v17_vulkan_gpu_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_vulkan_gpu_smoke PRIVATE moteur3d_v16_core Qt6::Gui)
    if(TARGET v17_vulkan_spirv)
        add_dependencies(v17_vulkan_gpu_smoke v17_vulkan_spirv)
        add_custom_command(TARGET v17_vulkan_gpu_smoke POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:v17_vulkan_gpu_smoke>/vulkan"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${V17_VULKAN_SPIRV}
                "$<TARGET_FILE_DIR:v17_vulkan_gpu_smoke>/vulkan"
            VERBATIM)
    endif()
    add_executable(v17_vulkan_ab_bench "vulkan/tests/vulkan_ab_bench.cpp")
    target_include_directories(v17_vulkan_ab_bench PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_vulkan_ab_bench PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
    if(TARGET v17_vulkan_spirv)
        add_dependencies(v17_vulkan_ab_bench v17_vulkan_spirv)
        add_custom_command(TARGET v17_vulkan_ab_bench POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:v17_vulkan_ab_bench>/vulkan"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${V17_VULKAN_SPIRV}
                "$<TARGET_FILE_DIR:v17_vulkan_ab_bench>/vulkan"
            VERBATIM)
    endif()
    # Khronos diagnostics run in the ORIGINAL Vulkan engine through native Win32 WSI.
    # Windows GUI subsystem prevents one persistent DOS window per test.
    # The native Vulkan render window is still visible and logs still redirect.
    add_executable(v17_vulkan_khronos_bench WIN32 "vulkan/tests/khronos_bench.cpp")
    target_include_directories(v17_vulkan_khronos_bench PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_vulkan_khronos_bench PRIVATE moteur3d_v16_core user32 gdi32)
    target_compile_definitions(v17_vulkan_khronos_bench PRIVATE CSE_WARP_BUILD_NUMBER=${V17_BUILD_NUMBER})
    if(MSVC)
        target_compile_options(v17_vulkan_khronos_bench PRIVATE /O2 /EHsc /utf-8)
    endif()
    if(TARGET v17_vulkan_spirv)
        add_dependencies(v17_vulkan_khronos_bench v17_vulkan_spirv)
        add_custom_command(TARGET v17_vulkan_khronos_bench POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:v17_vulkan_khronos_bench>/vulkan"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${V17_VULKAN_SPIRV} "$<TARGET_FILE_DIR:v17_vulkan_khronos_bench>/vulkan"
            VERBATIM)
    endif()
    # Independent console-subsystem diagnostic built from IDENTICAL source.
    # It inherits the calling BAT's cmd.exe console and helps identify GUI CRT
    # shutdown issues such as the HD620 0xC0000409 after a valid BMP.
    add_executable(v17_vulkan_khronos_console "vulkan/tests/khronos_bench.cpp")
    target_include_directories(v17_vulkan_khronos_console PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_vulkan_khronos_console PRIVATE moteur3d_v16_core user32 gdi32)
    target_compile_definitions(v17_vulkan_khronos_console PRIVATE CSE_WARP_BUILD_NUMBER=${V17_BUILD_NUMBER})
    if(MSVC)
        target_compile_options(v17_vulkan_khronos_console PRIVATE /O2 /EHsc /utf-8)
    endif()
    if(TARGET v17_vulkan_spirv)
        add_dependencies(v17_vulkan_khronos_console v17_vulkan_spirv)
        add_custom_command(TARGET v17_vulkan_khronos_console POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:v17_vulkan_khronos_console>/vulkan"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${V17_VULKAN_SPIRV} "$<TARGET_FILE_DIR:v17_vulkan_khronos_console>/vulkan"
            VERBATIM)
    endif()
    # CI builds on virtual GPU; real WSI timing results come from Intel HD620.
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${V17_MODULE_MANIFEST}"
      "$<TARGET_FILE_DIR:moteur3d_v17>/render_modules.json"
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
    # Package the separately versioned V17 reference scene. CI downloads the
    # latest validated release; local/offline builds use V17_SCENE_SOURCE_DIR's
    # checked-in fallback.
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${V17_SCENE_SOURCE_DIR}"
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
if(WIN32)
    add_executable(v17_route30 tools/route30.cpp)
    target_include_directories(v17_route30 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_route30 PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui user32)
    set_target_properties(v17_route30 PROPERTIES OUTPUT_NAME route30)
    if(TARGET v17_vulkan_spirv)
        add_dependencies(v17_route30 v17_vulkan_spirv)
        add_custom_command(TARGET v17_route30 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:v17_route30>/vulkan"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${V17_VULKAN_SPIRV} "$<TARGET_FILE_DIR:v17_route30>/vulkan"
            VERBATIM)
    endif()
endif()

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
install(TARGETS moteur3d_v17 moteur3d_v17_benchmark moteur3d_v17_campaign
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

# Escargot research targets only; never built by the default application target.
add_executable(v17_escargot_lab EXCLUDE_FROM_ALL tools/escargot_lab.cpp)
target_include_directories(v17_escargot_lab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(v17_escargot_lab PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
add_executable(v17_escargot_smoke EXCLUDE_FROM_ALL tests/escargot_smoke.cpp)
target_include_directories(v17_escargot_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(v17_escargot_smoke PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
# Deliberately no add_test: absent executable must not break existing ctest.

if(WIN32)
    add_executable(v17_escargot_ab EXCLUDE_FROM_ALL tools/escargot_ab.cpp)
    target_link_libraries(v17_escargot_ab PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
    add_executable(v17_escargot_explore_smoke EXCLUDE_FROM_ALL tools/escargot_explore_smoke.cpp moteur3d_v17_render_lumatrace.cpp)
    target_include_directories(v17_escargot_explore_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
    target_link_libraries(v17_escargot_explore_smoke PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui user32)
    add_executable(v17_escargot_scale EXCLUDE_FROM_ALL tools/escargot_scale.cpp)
    target_link_libraries(v17_escargot_scale PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
endif()

# Object-space reflection diagnostic; independent of the interactive renderer.
add_executable(v17_escargot_object_lab EXCLUDE_FROM_ALL tools/escargot_object_lab.cpp)
target_include_directories(v17_escargot_object_lab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(v17_escargot_object_lab PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)

add_executable(v17_escargot_object_generic EXCLUDE_FROM_ALL tools/escargot_object_generic.cpp)
target_include_directories(v17_escargot_object_generic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(v17_escargot_object_generic PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
