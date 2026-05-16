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
    src/app/SignalBus.h
    src/app/Application.h
    src/app/Application.cpp
    src/common/Config.h
    src/common/Config.cpp
    src/core/translate/TranslatorManager.cpp
    src/core/translate/plugins/DeepLTranslator.cpp
    src/core/translate/plugins/OpenAITranslator.cpp
    src/core/ocr/WindowsOcrEngine.cpp
    src/core/capture/ICaptureEngine.h
    src/core/capture/DxgiCaptureEngine.h
    src/core/capture/DxgiCaptureEngine.cpp
    src/engine/layout/LayoutEngine.cpp
    src/ui/overlay/TranslationOverlay.cpp
    src/ui/overlay/OverlayManager.cpp
    src/engine/hotkey/HotkeyManager.h
    src/engine/hotkey/HotkeyManager.cpp
    src/ui/tray/TrayManager.h
    src/ui/tray/TrayManager.cpp
    src/ui/selector/AreaSelector.h
    src/ui/selector/AreaSelector.cpp
    src/ui/settings/SettingsPanel.h
    src/ui/settings/SettingsPanel.cpp
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
    WindowsApp
)

set_target_properties(ScreenLingo PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_sources(ScreenLingo PRIVATE resources/dpiaware.manifest)

if(MSVC)
    target_compile_options(ScreenLingo PRIVATE /Zc:__cplusplus)
    target_compile_options(ScreenLingo PRIVATE /utf-8)
    target_compile_options(ScreenLingo PRIVATE /FS)
endif()
