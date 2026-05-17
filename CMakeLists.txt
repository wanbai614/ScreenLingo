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
    src/common/Types.h
    src/common/Config.h
    src/common/Config.cpp
    src/common/LanguageManager.h
    src/common/LanguageManager.cpp
    src/core/translate/ITranslator.h
    src/core/translate/TranslatorManager.cpp
    src/core/translate/plugins/DeepLTranslator.h
    src/core/translate/plugins/DeepLTranslator.cpp
    src/core/translate/plugins/OpenAITranslator.h
    src/core/translate/plugins/OpenAITranslator.cpp
    src/core/translate/plugins/DeepSeekTranslator.h
    src/core/translate/plugins/DeepSeekTranslator.cpp
    src/core/ocr/IOCREngine.h
    src/core/capture/ICaptureEngine.h
    src/core/capture/DxgiCaptureEngine.h
    src/core/capture/DxgiCaptureEngine.cpp
    src/engine/layout/LayoutEngine.cpp
    src/ui/overlay/TranslationOverlay.h
    src/ui/overlay/TranslationOverlay.cpp
    src/ui/overlay/OverlayManager.h
    src/ui/overlay/OverlayManager.cpp
    src/engine/hotkey/HotkeyManager.h
    src/engine/hotkey/HotkeyManager.cpp
    src/ui/tray/TrayManager.h
    src/ui/tray/TrayManager.cpp
    src/ui/selector/AreaSelector.h
    src/ui/selector/AreaSelector.cpp
    src/ui/settings/SettingsPanel.h
    src/ui/settings/SettingsPanel.cpp
    src/ui/settings/KeyCaptureDialog.h
    src/ui/settings/KeyCaptureDialog.cpp
)

# Windows OCR: WinRT/MSVC for real OCR, stub for MinGW development
if(MSVC)
    list(APPEND SOURCES src/core/ocr/WindowsOcrEngine.h src/core/ocr/WindowsOcrEngine.cpp)
else()
    list(APPEND SOURCES src/core/ocr/WindowsOcrEngine.h src/core/ocr/WindowsOcrEngineStub.cpp)
    message(WARNING "MinGW build: OCR engine will be a stub (no WinRT support). "
                    "Use MSVC for full OCR functionality.")
endif()

qt_add_executable(ScreenLingo ${SOURCES})

target_include_directories(ScreenLingo PRIVATE ${CMAKE_SOURCE_DIR}/src)

set(LINK_LIBS
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    d3d11
    dxgi
    user32
    gdi32
)

if(MSVC)
    list(APPEND LINK_LIBS WindowsApp)
endif()

target_link_libraries(ScreenLingo PRIVATE ${LINK_LIBS})

set_target_properties(ScreenLingo PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_sources(ScreenLingo PRIVATE resources/dpiaware.manifest)

# Translations
add_custom_command(
    TARGET ScreenLingo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ScreenLingo>/translations
    COMMAND ${QT6_INSTALL_PREFIX}/bin/lrelease ${CMAKE_SOURCE_DIR}/resources/screenlingo_zh_CN.ts
            -qm $<TARGET_FILE_DIR:ScreenLingo>/translations/screenlingo_zh_CN.qm
    COMMENT "Compiling translation: screenlingo_zh_CN.qm"
)

if(MSVC)
    target_compile_options(ScreenLingo PRIVATE /Zc:__cplusplus /utf-8 /FS)
endif()
