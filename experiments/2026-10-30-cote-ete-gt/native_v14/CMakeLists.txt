cmake_minimum_required(VERSION 3.20)
project(Moteur3DNative VERSION 15.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Core)

set(M3D_CORE_SOURCES
    src/m3d_scene.cpp
    src/m3d_scene_v16.cpp
    src/m3d_scene_v16_compile.cpp
    src/m3d_lighting.cpp
    src/m3d_intersection.cpp
    src/m3d_shading.cpp
    src/m3d_render.cpp
    src/m3d_hybrid.cpp
    src/m3d_post.cpp
    src/m3d_simd.cpp
    src/m3d_compute11.cpp
    src/m3d_directx11.cpp
    src/m3d_dxr.cpp
    src/simd/avx2/post.cpp
    src/simd/avx2/ray_generation.cpp
    src/simd/avx2/triangle_intersection.cpp
    src/simd/avx2/lighting.cpp
    src/simd/avx2/texture_sampling.cpp
)

add_library(m3d_core STATIC ${M3D_CORE_SOURCES})
if(WIN32)
    target_link_libraries(m3d_core PUBLIC d3d11 d3d12 dxgi d3dcompiler)
endif()
target_include_directories(m3d_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(m3d_core PUBLIC Threads::Threads Qt6::Core)

qt_add_executable(moteur3d_qt WIN32 src/moteur3d_qt.cpp)
target_link_libraries(moteur3d_qt PRIVATE m3d_core Qt6::Widgets)
if(WIN32)
    add_executable(moteur3d_win32 WIN32 src/moteur3d_win32.cpp)
    target_link_libraries(moteur3d_win32 PRIVATE m3d_core comctl32 comdlg32 gdi32 user32)
endif()

if(MSVC)
    set_source_files_properties(src/simd/avx2/post.cpp src/simd/avx2/ray_generation.cpp src/simd/avx2/triangle_intersection.cpp src/simd/avx2/lighting.cpp src/simd/avx2/texture_sampling.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    target_compile_options(m3d_core PRIVATE /O2 /EHsc /utf-8)
    target_compile_options(moteur3d_qt PRIVATE /O2 /EHsc /utf-8)
else()
    set_source_files_properties(src/simd/avx2/post.cpp src/simd/avx2/ray_generation.cpp src/simd/avx2/triangle_intersection.cpp src/simd/avx2/lighting.cpp src/simd/avx2/texture_sampling.cpp PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
    target_compile_options(m3d_core PRIVATE -O2 -Wall -Wextra -Wpedantic)
    target_compile_options(moteur3d_qt PRIVATE -O2 -Wall -Wextra -Wpedantic)
endif()

install(TARGETS moteur3d_qt RUNTIME DESTINATION .)
if(WIN32)
    install(TARGETS moteur3d_win32 RUNTIME DESTINATION .)
endif()
install(FILES src/shaders/m3d_raytrace_compute.hlsl DESTINATION shaders)
install(FILES moteur3d_v1314_scene.bin DESTINATION . OPTIONAL)

if(WIN32)
    add_custom_command(TARGET m3d_core POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/m3d_raytrace_compute.hlsl" "${CMAKE_CURRENT_BINARY_DIR}/shaders/m3d_raytrace_compute.hlsl")
endif()
