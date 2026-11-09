cmake_minimum_required(VERSION 3.24)
project(Moteur3D_V16 VERSION 16.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

set(NATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
set(NATIVE_SRC "${NATIVE}/src")
set(M3D_RENDER_SOURCES
    "${NATIVE_SRC}/m3d_scene_v16.cpp"
    "${NATIVE_SRC}/m3d_v16_bridge.cpp"
    "${NATIVE_SRC}/m3d_lighting.cpp"
    "${NATIVE_SRC}/m3d_intersection.cpp"
    "${NATIVE_SRC}/m3d_shading.cpp"
    "${NATIVE_SRC}/m3d_render.cpp"
    "${NATIVE_SRC}/m3d_hybrid.cpp"
    "${NATIVE_SRC}/m3d_post.cpp"
    "${NATIVE_SRC}/m3d_simd.cpp"
    "${NATIVE_SRC}/m3d_compute11.cpp"
    "${NATIVE_SRC}/m3d_directx11.cpp"
    "${NATIVE_SRC}/m3d_dxr.cpp"
    "${NATIVE_SRC}/simd/avx2/post.cpp"
    "${NATIVE_SRC}/simd/avx2/ray_generation.cpp"
    "${NATIVE_SRC}/simd/avx2/triangle_intersection.cpp"
    "${NATIVE_SRC}/simd/avx2/lighting.cpp"
    "${NATIVE_SRC}/simd/avx2/texture_sampling.cpp"
)
add_library(moteur3d_v16_core SHARED ${M3D_RENDER_SOURCES})
if(WIN32)
    set_target_properties(moteur3d_v16_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
target_include_directories(moteur3d_v16_core PUBLIC "${NATIVE}")
target_link_libraries(moteur3d_v16_core PUBLIC Threads::Threads Qt6::Core)
if(WIN32)
    target_link_libraries(moteur3d_v16_core PUBLIC d3d11 d3d12 dxgi d3dcompiler)
endif()
set(M3D_AVX_SOURCES
    "${NATIVE_SRC}/simd/avx2/post.cpp"
    "${NATIVE_SRC}/simd/avx2/ray_generation.cpp"
    "${NATIVE_SRC}/simd/avx2/triangle_intersection.cpp"
    "${NATIVE_SRC}/simd/avx2/lighting.cpp"
    "${NATIVE_SRC}/simd/avx2/texture_sampling.cpp"
)
if(MSVC)
    set_source_files_properties(${M3D_AVX_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    target_compile_options(moteur3d_v16_core PRIVATE /O2 /EHsc /utf-8)
else()
    set_source_files_properties(${M3D_AVX_SOURCES} PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
    target_compile_options(moteur3d_v16_core PRIVATE -O2 -Wall -Wextra -Wpedantic)
endif()
qt_add_executable(moteur3d_v16 WIN32 "${NATIVE_SRC}/moteur3d_v16_qt.cpp")
target_link_libraries(moteur3d_v16 PRIVATE moteur3d_v16_core Qt6::Widgets Qt6::Concurrent)
if(MSVC)
    target_compile_options(moteur3d_v16 PRIVATE /O2 /EHsc /utf-8)
endif()
# Standalone frontends: rebuilding controls or benchmark does not recompile the engine DLL.
add_executable(moteur3d_v16_benchmark "${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark.cpp")
target_link_libraries(moteur3d_v16_benchmark PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
qt_add_executable(moteur3d_v16_benchui WIN32 "${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark_ui.cpp")
target_link_libraries(moteur3d_v16_benchui PRIVATE Qt6::Widgets)
if(MSVC)
    target_compile_options(moteur3d_v16_benchmark PRIVATE /O2 /EHsc /utf-8)
    target_compile_options(moteur3d_v16_benchui PRIVATE /O2 /EHsc /utf-8)
endif()
add_executable(v16_scene_check "${CMAKE_CURRENT_SOURCE_DIR}/tests/scene_check.cpp")
target_link_libraries(v16_scene_check PRIVATE moteur3d_v16_core Qt6::Core)
if(MSVC)
    target_compile_options(v16_scene_check PRIVATE /O2 /EHsc /utf-8)
endif()
add_executable(v16_render_smoke "${CMAKE_CURRENT_SOURCE_DIR}/tests/render_smoke.cpp")
target_link_libraries(v16_render_smoke PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
if(MSVC)
    target_compile_options(v16_render_smoke PRIVATE /O2 /EHsc /utf-8)
endif()
enable_testing()
add_test(NAME v16_pavillon01_loader
    COMMAND v16_scene_check "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01/scene.json")
set_tests_properties(v16_pavillon01_loader PROPERTIES TIMEOUT 240)
add_test(NAME v16_pavillon01_cpu_render
    COMMAND v16_render_smoke "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01/scene.json" "${CMAKE_CURRENT_BINARY_DIR}/pavillon01-smoke.png")
set_tests_properties(v16_pavillon01_cpu_render PROPERTIES TIMEOUT 240)
# Independent scene uses precisely the same C++ renderer and PBR loader.
add_test(NAME v16_material_lab_cpu_render
    COMMAND v16_render_smoke "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/material-lab/scene.json"
        "${CMAKE_CURRENT_BINARY_DIR}/material-lab-cpu.png")
set_tests_properties(v16_material_lab_cpu_render PROPERTIES TIMEOUT 240)
add_executable(v16_gi_smoke "${CMAKE_CURRENT_SOURCE_DIR}/tests/gi_smoke.cpp")
target_link_libraries(v16_gi_smoke PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
if(MSVC)
    target_compile_options(v16_gi_smoke PRIVATE /O2 /EHsc /utf-8)
endif()
add_test(NAME v16_cpu_global_illumination_fullhd
    COMMAND v16_gi_smoke "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01/scene.json"
        "${CMAKE_CURRENT_BINARY_DIR}/pavillon01-gi-smoke.png")
set_tests_properties(v16_cpu_global_illumination_fullhd PROPERTIES TIMEOUT 240)
add_test(NAME v16_material_lab_cpu_gi
    COMMAND v16_gi_smoke "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/material-lab/scene.json"
        "${CMAKE_CURRENT_BINARY_DIR}/material-lab-gi.png")
set_tests_properties(v16_material_lab_cpu_gi PROPERTIES TIMEOUT 240)
add_test(NAME v16_benchmark_quick
    COMMAND moteur3d_v16_benchmark
        "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01/scene.json"
        "${CMAKE_CURRENT_BINARY_DIR}/benchmark-tests" --quick)
set_tests_properties(v16_benchmark_quick PROPERTIES TIMEOUT 240)
if(WIN32)
    add_executable(v16_gpu_warp_smoke "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpu_warp_smoke.cpp")
    target_link_libraries(v16_gpu_warp_smoke PRIVATE moteur3d_v16_core Qt6::Core Qt6::Gui)
    if(MSVC)
        target_compile_options(v16_gpu_warp_smoke PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME v16_gpu_compute_warp
        COMMAND v16_gpu_warp_smoke "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01/scene.json"
            "${CMAKE_CURRENT_BINARY_DIR}/pavillon01-gpu-warp.png")
    set_tests_properties(v16_gpu_compute_warp
        PROPERTIES TIMEOUT 240 ENVIRONMENT "M3D_V16_FORCE_WARP=1")
    add_test(NAME v16_material_lab_gpu_warp
        COMMAND v16_gpu_warp_smoke "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/material-lab/scene.json"
            "${CMAKE_CURRENT_BINARY_DIR}/material-lab-gpu.png")
    set_tests_properties(v16_material_lab_gpu_warp
        PROPERTIES TIMEOUT 240 ENVIRONMENT "M3D_V16_FORCE_WARP=1")
endif()
if(WIN32)
    add_executable(v16_shader_check "${CMAKE_CURRENT_SOURCE_DIR}/tests/shader_check.cpp")
    target_link_libraries(v16_shader_check PRIVATE d3dcompiler)
    add_test(NAME v16_hlsl_compute_shader
        COMMAND v16_shader_check "${NATIVE_SRC}/shaders/m3d_raytrace_compute.hlsl")
    set_tests_properties(v16_hlsl_compute_shader PROPERTIES TIMEOUT 60)
endif()
add_custom_command(TARGET moteur3d_v16 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01"
        "$<TARGET_FILE_DIR:moteur3d_v16>/pavillon01"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/material-lab"
        "$<TARGET_FILE_DIR:moteur3d_v16>/material-lab"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:moteur3d_v16>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${NATIVE_SRC}/shaders/m3d_raytrace_compute.hlsl"
        "$<TARGET_FILE_DIR:moteur3d_v16>/shaders/m3d_raytrace_compute.hlsl"
    VERBATIM)
install(TARGETS moteur3d_v16 moteur3d_v16_core moteur3d_v16_benchmark moteur3d_v16_benchui
    RUNTIME DESTINATION . LIBRARY DESTINATION . ARCHIVE DESTINATION lib)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01" DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/material-lab" DESTINATION .)
install(FILES "${NATIVE_SRC}/shaders/m3d_raytrace_compute.hlsl" DESTINATION shaders)
# No V3/V4/V5 binary loader, exporter or legacy Qt application is linked into V16.
