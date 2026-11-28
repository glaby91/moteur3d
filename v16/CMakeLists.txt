cmake_minimum_required(VERSION 3.24)
file(READ "${CMAKE_CURRENT_LIST_DIR}/version.json" V16_VERSION_JSON)
string(JSON V16_PRODUCT_VERSION GET "${V16_VERSION_JSON}" engineVersion)
project(Moteur3D_V16 VERSION ${V16_PRODUCT_VERSION} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

set(M3D_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(M3D_MODULE_MANIFEST "${M3D_REPOSITORY_ROOT}/engine/render_modules.json")
include("${M3D_REPOSITORY_ROOT}/cmake/RenderModules.cmake")
m3d_validate_render_modules("${M3D_MODULE_MANIFEST}" "${M3D_REPOSITORY_ROOT}")
m3d_render_module_sources("${M3D_MODULE_MANIFEST}" v16
    "${M3D_REPOSITORY_ROOT}" M3D_RENDER_SOURCES)
set(NATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
set(NATIVE_SRC "${NATIVE}/src")
m3d_render_module_source("${M3D_MODULE_MANIFEST}" vulkan_raster 0
    "${M3D_REPOSITORY_ROOT}" M3D_VULKAN_SOURCE)
m3d_render_module_source("${M3D_MODULE_MANIFEST}" vulkan_raster 1
    "${M3D_REPOSITORY_ROOT}" M3D_VULKAN_STUB_SOURCE)
m3d_render_module_source("${M3D_MODULE_MANIFEST}" vulkan_resources 0
    "${M3D_REPOSITORY_ROOT}" M3D_VULKAN_RESOURCES_SOURCE)
m3d_render_module_source("${M3D_MODULE_MANIFEST}" lumatrace_gpu_compute 0
    "${M3D_REPOSITORY_ROOT}" M3D_LUMATRACE_GPU_SOURCE)
configure_file("${M3D_MODULE_MANIFEST}"
    "${CMAKE_CURRENT_BINARY_DIR}/render_modules.json" COPYONLY)
# ARM64 keeps the original scalar CPU path; AVX2 units are x86-only.
if(ANDROID)
    # Both diagnostic emulator and ARM64 tablet use the scalar reference.
    list(FILTER M3D_RENDER_SOURCES EXCLUDE REGEX "/src/simd/avx2/")
elseif(APPLE AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    list(FILTER M3D_RENDER_SOURCES EXCLUDE REGEX "/src/simd/avx2/")
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$" AND NOT CMAKE_OSX_ARCHITECTURES)
    list(FILTER M3D_RENDER_SOURCES EXCLUDE REGEX "/src/simd/avx2/")
endif()
add_library(moteur3d_v16_core SHARED ${M3D_RENDER_SOURCES})
if(ANDROID AND ANDROID_ABI STREQUAL "x86_64")
    # Dedicated emulator artifact; ARM64 and desktop never get this definition.
    target_compile_definitions(moteur3d_v16_core PRIVATE CSE_ANDROID_EMULATOR_TEST=1)
endif()
if(WIN32)
    set_target_properties(moteur3d_v16_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
target_include_directories(moteur3d_v16_core PUBLIC "${NATIVE}")
target_link_libraries(moteur3d_v16_core PUBLIC Threads::Threads Qt6::Core Qt6::Gui)
if(WIN32)
    # LumaTrace compute is an independent optional backend module, separate
    # from the DX11 raster frame path that consumes its radiance buffer.
    target_sources(moteur3d_v16_core PRIVATE "${M3D_LUMATRACE_GPU_SOURCE}")
    target_link_libraries(moteur3d_v16_core PUBLIC d3d11 d3d12 dxgi d3dcompiler)
endif()
if(WIN32 OR APPLE OR ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Vulkan renderer lives in the shared engine library on Windows and Linux.
    # An optional backend must not break the stock DX11-only V17 build.
    option(M3D_REQUIRE_VULKAN "Require Vulkan SDK 1.3.204+ for native raster" OFF)
    find_package(Vulkan 1.3.204 QUIET)
    if(M3D_REQUIRE_VULKAN AND NOT Vulkan_FOUND)
        message(FATAL_ERROR "Vulkan SDK 1.3.204+ is mandatory for this build")
    endif()
    if(Vulkan_FOUND)
        target_sources(moteur3d_v16_core PRIVATE
            "${M3D_VULKAN_SOURCE}"
            "${M3D_VULKAN_RESOURCES_SOURCE}")
        target_include_directories(moteur3d_v16_core PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../v17/vulkan")
        target_link_libraries(moteur3d_v16_core PRIVATE Vulkan::Vulkan)
        # Vulkan is optional at runtime. Avoid an eager vulkan-1.dll import,
        # otherwise ALL V17.493 CPU and DX11 tools fail with STATUS_DLL_NOT_FOUND
        # on clean CI or systems without the Vulkan runtime installed.
        if(MSVC)
            target_link_options(moteur3d_v16_core PRIVATE "/DELAYLOAD:vulkan-1.dll")
            target_link_libraries(moteur3d_v16_core PRIVATE delayimp)
        endif()
        target_compile_definitions(moteur3d_v16_core PUBLIC M3D_V17_VULKAN_ENABLED=1)
    else()
        target_sources(moteur3d_v16_core PRIVATE
            "${M3D_VULKAN_STUB_SOURCE}")
        target_compile_definitions(moteur3d_v16_core PUBLIC M3D_V17_VULKAN_ENABLED=0)
    endif()
else()
    target_sources(moteur3d_v16_core PRIVATE
        "${M3D_VULKAN_STUB_SOURCE}")
    target_compile_definitions(moteur3d_v16_core PUBLIC M3D_V17_VULKAN_ENABLED=0)
endif()
set(M3D_AVX_SOURCES)
foreach(M3D_SOURCE IN LISTS M3D_RENDER_SOURCES)
    if(M3D_SOURCE MATCHES "/src/simd/avx2/")
        list(APPEND M3D_AVX_SOURCES "${M3D_SOURCE}")
    endif()
endforeach()
if(MSVC)
    set_source_files_properties(${M3D_AVX_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    target_compile_options(moteur3d_v16_core PRIVATE /O2 /EHsc /utf-8)
else()
    if(M3D_AVX_SOURCES)
        set_source_files_properties(${M3D_AVX_SOURCES} PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
    endif()
    target_compile_options(moteur3d_v16_core PRIVATE -O2 -Wall -Wextra -Wpedantic)
endif()
if(ANDROID)
    # Android APK has its own frontend; no desktop CLI executables are deployed.
    return()
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
add_executable(v16_photo2scene_proof "${CMAKE_CURRENT_SOURCE_DIR}/tests/photo2scene_proof.cpp")
target_link_libraries(v16_photo2scene_proof PRIVATE moteur3d_v16_core Qt6::Core)
if(MSVC)
    target_compile_options(v16_photo2scene_proof PRIVATE /O2 /EHsc /utf-8)
endif()
enable_testing()
add_test(NAME v16_photo2scene_facade_scene_loader
    COMMAND v16_photo2scene_proof "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pripyat-photo2scene-proof/scene.json")
set_tests_properties(v16_photo2scene_facade_scene_loader PROPERTIES TIMEOUT 120)
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
    set_tests_properties(v16_hlsl_compute_shader PROPERTIES TIMEOUT 240)
endif()
add_custom_command(TARGET moteur3d_v16 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01"
        "$<TARGET_FILE_DIR:moteur3d_v16>/pavillon01"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/material-lab"
        "$<TARGET_FILE_DIR:moteur3d_v16>/material-lab"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pripyat-photo2scene-proof"
        "$<TARGET_FILE_DIR:moteur3d_v16>/pripyat-photo2scene-proof"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:moteur3d_v16>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${NATIVE_SRC}/shaders/m3d_raytrace_compute.hlsl"
        "$<TARGET_FILE_DIR:moteur3d_v16>/shaders/m3d_raytrace_compute.hlsl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${M3D_MODULE_MANIFEST}"
        "$<TARGET_FILE_DIR:moteur3d_v16>/render_modules.json"
    VERBATIM)
install(TARGETS moteur3d_v16 moteur3d_v16_core moteur3d_v16_benchmark moteur3d_v16_benchui
    RUNTIME DESTINATION . LIBRARY DESTINATION . ARCHIVE DESTINATION lib)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pavillon01" DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/material-lab" DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../scenes/v16/pripyat-photo2scene-proof" DESTINATION .)
install(FILES "${NATIVE_SRC}/shaders/m3d_raytrace_compute.hlsl" DESTINATION shaders)
install(FILES "${M3D_MODULE_MANIFEST}" DESTINATION .)
# No V3/V4/V5 binary loader, exporter or legacy Qt application is linked into V16.
