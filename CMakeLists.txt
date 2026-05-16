cmake_minimum_required(VERSION 3.22)
project(ScreenLingo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    src/main.cpp
)

qt_add_executable(ScreenLingo ${SOURCES})

target_include_directories(ScreenLingo PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(ScreenLingo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    d3d11
    dxgi
    user32
    gdi32
)

set_target_properties(ScreenLingo PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_sources(ScreenLingo PRIVATE resources/dpiaware.manifest)

if(MSVC)
    target_compile_options(ScreenLingo PRIVATE /Zc:__cplusplus)
    target_compile_options(ScreenLingo PRIVATE /utf-8)
endif()
