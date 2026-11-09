cmake_minimum_required(VERSION 3.24)
project(Moteur3D_V16 VERSION 16.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets Concurrent)

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
add_library(moteur3d_v16_core STATIC ${M3D_RENDER_SOURCES})
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
add_executable(v16_scene_check "${CMAKE_CURRENT_SOURCE_DIR}/tests/scene_check.cpp")
target_link_libraries(v16_scene_check PRIVATE moteur3d_v16_core Qt6::Core)
if(MSVC)
    target_compile_options(v16_scene_check PRIVATE /O2 /EHsc /utf-8)
endif()
enable_testing()
add_test(NAME v16_pavillon01_loader
    COMMAND v16_scene_check "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01/scene.json")
set_tests_properties(v16_pavillon01_loader PROPERTIES TIMEOUT 240)
add_custom_command(TARGET moteur3d_v16 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01"
        "$<TARGET_FILE_DIR:moteur3d_v16>/pavillon01"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:moteur3d_v16>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${NATIVE_SRC}/shaders/m3d_raytrace_compute.hlsl"
        "$<TARGET_FILE_DIR:moteur3d_v16>/shaders/m3d_raytrace_compute.hlsl"
    VERBATIM)
install(TARGETS moteur3d_v16 RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01" DESTINATION .)
install(FILES "${NATIVE_SRC}/shaders/m3d_raytrace_compute.hlsl" DESTINATION shaders)
# No V3/V4/V5 binary loader, exporter or legacy Qt application is linked into V16.
