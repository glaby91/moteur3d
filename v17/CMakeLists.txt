cmake_minimum_required(VERSION 3.24)
project(CSE_WARP_V17 VERSION 17.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# Reuse the proven V16 CPU, Compute and raster engines, plus their tests.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../v16" "${CMAKE_CURRENT_BINARY_DIR}/v16")

qt_add_executable(moteur3d_v17 WIN32 "${CMAKE_CURRENT_SOURCE_DIR}/moteur3d_v17_qt.cpp")
target_include_directories(moteur3d_v17 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14")
target_link_libraries(moteur3d_v17 PRIVATE moteur3d_v16_core Qt6::Widgets Qt6::Concurrent)
if(MSVC)
    target_compile_options(moteur3d_v17 PRIVATE /O2 /EHsc /utf-8)
endif()
add_custom_command(TARGET moteur3d_v17 POST_BUILD
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
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:moteur3d_v17>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../experiments/2026-10-30-cote-ete-gt/native_v14/src/shaders/m3d_raytrace_compute.hlsl"
      "$<TARGET_FILE_DIR:moteur3d_v17>/shaders/m3d_raytrace_compute.hlsl"
    VERBATIM)
install(TARGETS moteur3d_v17 RUNTIME DESTINATION .)
