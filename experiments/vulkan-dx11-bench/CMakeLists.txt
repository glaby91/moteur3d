cmake_minimum_required(VERSION 3.21)
project(cse_warp_vulkan_vs_dx11 LANGUAGES CXX)
if(NOT WIN32)
    message(FATAL_ERROR "This experimental benchmark compares Windows Direct3D11 with Vulkan; Windows only.")
endif()
find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR NAMES glslangValidator glslangValidator.exe
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32" REQUIRED)
set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/benchmark.comp.spv")
add_custom_command(OUTPUT "${SPIRV}"
  COMMAND "${GLSLANG_VALIDATOR}" -V --target-env vulkan1.0 "${CMAKE_CURRENT_SOURCE_DIR}/shaders/benchmark.comp" -o "${SPIRV}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/benchmark.comp"
  COMMENT "Compile Vulkan compute shader to SPIR-V" VERBATIM)
add_custom_target(cse_warp_vk_spirv DEPENDS "${SPIRV}")
add_executable(cse_warp_vk_dx11_bench src/main.cpp)
target_compile_features(cse_warp_vk_dx11_bench PRIVATE cxx_std_17)
target_compile_definitions(cse_warp_vk_dx11_bench PRIVATE UNICODE _UNICODE)
target_include_directories(cse_warp_vk_dx11_bench PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(cse_warp_vk_dx11_bench PRIVATE Vulkan::Vulkan d3d11 dxgi d3dcompiler)
if(MSVC)
  target_compile_options(cse_warp_vk_dx11_bench PRIVATE /W4 /O2 /EHsc /utf-8)
endif()
add_dependencies(cse_warp_vk_dx11_bench cse_warp_vk_spirv)
add_custom_command(TARGET cse_warp_vk_dx11_bench POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SPIRV}" "$<TARGET_FILE_DIR:cse_warp_vk_dx11_bench>/benchmark.comp.spv"
    VERBATIM)
