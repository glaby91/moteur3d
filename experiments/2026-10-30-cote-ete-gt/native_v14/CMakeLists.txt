cmake_minimum_required(VERSION 3.20)
project(Moteur3DNative VERSION 16.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Core)

set(M3D_CORE_SOURCES
    src/m3d_scene_v16.cpp
    src/m3d_scene_v16_compile.cpp
    src/m3d_pbr_v16.cpp
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
    target_link_libraries(moteur3d_qt PRIVATE winmm)

    # Automatic, dependency-free creation of an ORIGINAL technical test WAV.
    # Production music files are not embedded or fetched during build.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(M3D_RADIO_WAV "${CMAKE_CURRENT_BINARY_DIR}/audio/pripyat_radio.wav")
    add_custom_command(
        OUTPUT "${M3D_RADIO_WAV}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/audio"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../../../assets/audio/pripyat_radio/generate_test_wav.py"
                "${M3D_RADIO_WAV}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../../assets/audio/pripyat_radio/generate_test_wav.py"
        COMMENT "Generating optional 30-second CSE WARP radio preview"
        VERBATIM
    )
    add_custom_target(m3d_radio_sample ALL DEPENDS "${M3D_RADIO_WAV}")
    add_dependencies(moteur3d_qt m3d_radio_sample)
    install(FILES "${M3D_RADIO_WAV}" DESTINATION audio)
endif()

enable_testing()
add_executable(m3d_v16_scene_test tests/test_v16_scene.cpp)
target_link_libraries(m3d_v16_scene_test PRIVATE m3d_core Qt6::Core)
add_test(NAME pavillon01_scene_compile COMMAND m3d_v16_scene_test ${CMAKE_CURRENT_SOURCE_DIR}/../../../scenes/v16/pavillon01/scene.json)

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
install(FILES src/shaders/m3d_raytrace_compute.hlsl DESTINATION shaders)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../scenes/v16/pavillon01/ DESTINATION scenes/v16/pavillon01)

if(WIN32)
    add_custom_command(TARGET m3d_core POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/m3d_raytrace_compute.hlsl" "${CMAKE_CURRENT_BINARY_DIR}/shaders/m3d_raytrace_compute.hlsl")
endif()
