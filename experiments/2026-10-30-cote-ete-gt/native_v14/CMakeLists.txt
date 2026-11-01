cmake_minimum_required(VERSION 3.20)
project(Moteur3DNative VERSION 14.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

set(M3D_CORE_SOURCES
    src/m3d_scene.cpp
    src/m3d_lighting.cpp
    src/m3d_intersection.cpp
    src/m3d_shading.cpp
    src/m3d_render.cpp
    src/m3d_post.cpp
)

add_library(m3d_core STATIC ${M3D_CORE_SOURCES})
target_include_directories(m3d_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(m3d_core PUBLIC Threads::Threads)

add_executable(moteur3d_qt src/moteur3d_qt.cpp)
target_link_libraries(moteur3d_qt PRIVATE m3d_core Qt6::Widgets)

if(MSVC)
    target_compile_options(m3d_core PRIVATE /O2 /EHsc /utf-8)
    target_compile_options(moteur3d_qt PRIVATE /O2 /EHsc /utf-8)
else()
    target_compile_options(m3d_core PRIVATE -O2 -Wall -Wextra -Wpedantic)
    target_compile_options(moteur3d_qt PRIVATE -O2 -Wall -Wextra -Wpedantic)
endif()

install(TARGETS moteur3d_qt RUNTIME DESTINATION .)
install(FILES moteur3d_v1314_scene.bin DESTINATION . OPTIONAL)
