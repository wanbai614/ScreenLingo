# ScreenLingo Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the ScreenLingo v1.0 core closed-loop pipeline — area selection → screen capture → OCR → translation → overlay display with 3-tier layout, hotkey control, system tray, and settings panel.

**Architecture:** Modular plugin architecture with SignalBus decoupling. Core modules (capture, OCR, translate) expose pure virtual interfaces. Engine modules (layout, hotkey) are standalone. UI modules (overlay, selector, settings, tray) consume signals from core. Application class wires everything at startup, managing lifecycle and error handling.

**Tech Stack:** Qt 6.x, C++20, CMake, DXGI Desktop Duplication API, Windows.Media.Ocr (WinRT), QNetworkAccessManager for HTTP translation APIs.

**Dependency Order:** common → interfaces → core implementations → engine → UI → Application integration

---

---

### Task 1: Project Scaffolding — CMake, main skeleton, .gitignore

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp` (skeleton)
- Create: `.gitignore`

- [ ] **Step 1: Write CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)
project(ScreenLingo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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
    WindowsApp   # WinRT for Windows.Media.Ocr
    user32       # RegisterHotKey
    gdi32        # Display DC
)

# Windows-specific: enable WinRT
set_target_properties(ScreenLingo PROPERTIES
    WIN32_EXECUTABLE TRUE
    CXX_STANDARD 20
)

# Enable Per-Monitor DPI awareness v2 via manifest
target_sources(ScreenLingo PRIVATE resources/dpiaware.manifest)
```

- [ ] **Step 2: Write skeleton main.cpp**

```cpp
#include <QtWidgets/QApplication>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationName("ScreenLingo");
    app.setOrganizationName("ScreenLingo");
    app.setQuitOnLastWindowClosed(false);

    // TODO: Wire up Application class in Task 14

    return app.exec();
}
```

- [ ] **Step 3: Write .gitignore**

```
build/
cmake-build-*/
.vs/
.vscode/
*.user
*.autosave
Debug/
Release/
*.exe
*.dll
*.pdb
*.ilk
*.obj
.DS_Store
Thumbs.db
```

- [ ] **Step 4: Write DPI awareness manifest**

Create `resources/dpiaware.manifest`:
```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
    <application xmlns="urn:schemas-microsoft-com:asm.v3">
        <windowsSettings>
            <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
            <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware>
        </windowsSettings>
    </application>
</assembly>
```

- [ ] **Step 5: Configure and verify build**

Run: `cmake -B build -S . -DCMAKE_PREFIX_PATH=<path-to-qt6>`
Run: `cmake --build build`
Expected: Build succeeds with skeleton main.cpp

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/main.cpp .gitignore resources/dpiaware.manifest
git commit -m "chore: project scaffolding with Qt6/CMake/DXGI"
```

---

### Task 2: Common Types

**Files:**
- Create: `src/common/Types.h`

- [ ] **Step 1: Write Types.h**

```cpp
#pragma once

#include <QtCore/QRect>
#include <QtCore/QPoint>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QColor>
#include <QtGui/QFont>

// --- Work Modes ---
enum class Mode {
    RealTime,
    Snapshot,
    Pause
};

// --- OCR Result ---
struct TextBox {
    QString text;
    QRect   boundingRect;  // relative to captured region
};

struct OCRResult {
    QString         fullText;
    QVector<TextBox> boxes;
};

// --- Translation ---
struct TranslateRequest {
    QString text;
    QString sourceLang;  // "auto" for auto-detect
    QString targetLang;
};

// --- Layout ---
struct LayoutRequest {
    QRect   sourceRect;        // original text screen position
    QString translatedText;
    int     preferredFontSize;
};

struct LayoutResult {
    QPoint  position;
    int     fontSize;
    int     maxWidth;
    bool    isTruncated;
};

// --- Style ---
struct StyleConfig {
    QColor textColor       = Qt::white;
    QColor backgroundColor = QColor(51, 51, 51, 179);  // #333 with ~70% alpha
    int    backgroundAlpha = 70;   // 0-100
    int    borderRadius    = 6;
    QColor borderColor     = QColor(102, 102, 102);
    int    borderWidth     = 1;
    QFont  font            = QFont("Microsoft YaHei", 14);
};

// --- Selection Area ---
struct SelectionArea {
    int   id;
    int   screenIndex;
    QRect geometry;
    bool  enabled = true;
};

// --- Hotkey ---
struct HotkeyBinding {
    QString id;
    QString label;
    QString defaultKeys;
    QString currentKeys;
};
```

- [ ] **Step 2: Commit**

```bash
git add src/common/Types.h
git commit -m "feat: add shared data types for all modules"
```

---

### Task 3: SignalBus

**Files:**
- Create: `src/app/SignalBus.h`

- [ ] **Step 1: Write SignalBus.h**

```cpp
#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>
#include "common/Types.h"

class SignalBus : public QObject {
    Q_OBJECT

public:
    explicit SignalBus(QObject* parent = nullptr) : QObject(parent) {}

signals:
    // Capture → OCR
    void frameReady(const QImage& frame, const QRect& region);

    // OCR → Translate
    void ocrCompleted(const OCRResult& result);

    // Translate → Layout → Overlay
    void translationReady(const QString& original, const QString& translated,
                          const QRect& sourceRect);

    // Mode changes
    void modeChanged(Mode newMode);

    // Style changes (Settings → all Overlays)
    void styleChanged(const StyleConfig& style);

    // Global visibility
    void globalVisibilityChanged(bool visible);

    // Translation errors
    void translationError(const QString& text, const QString& error);

    // Area selection completed
    void areaConfirmed(const SelectionArea& area);

    // Snapshot trigger
    void snapshotRequested();
};
```

- [ ] **Step 2: Commit**

```bash
git add src/app/SignalBus.h
git commit -m "feat: add SignalBus for cross-module decoupling"
```

---

### Task 4: Config Persistence

**Files:**
- Create: `src/common/Config.h`
- Create: `src/common/Config.cpp`

- [ ] **Step 1: Write Config.h**

```cpp
#pragma once

#include <QtCore/QSettings>
#include <QtCore/QString>
#include "Types.h"

class Config {
public:
    explicit Config(const QString& org, const QString& app);

    // Style
    StyleConfig loadStyle() const;
    void saveStyle(const StyleConfig& style);

    // Language
    QString sourceLang() const;
    void setSourceLang(const QString& lang);
    QString targetLang() const;
    void setTargetLang(const QString& lang);

    // Mode
    Mode lastMode() const;
    void setLastMode(Mode mode);

    // Active translator
    QString activeTranslator() const;
    void setActiveTranslator(const QString& name);

    // Selected areas
    QVector<SelectionArea> loadAreas() const;
    void saveAreas(const QVector<SelectionArea>& areas);

    // Hotkeys
    QVector<HotkeyBinding> loadHotkeys(const QVector<HotkeyBinding>& defaults) const;
    void saveHotkeys(const QVector<HotkeyBinding>& bindings);

    // Per-translator config (non-secret: endpoints, models, etc.)
    QString translatorConfig(const QString& pluginName, const QString& key) const;
    void setTranslatorConfig(const QString& pluginName, const QString& key,
                             const QString& value);

private:
    QSettings m_settings;
};
```

- [ ] **Step 2: Write Config.cpp**

```cpp
#include "Config.h"

Config::Config(const QString& org, const QString& app)
    : m_settings(org, app) {}

StyleConfig Config::loadStyle() const {
    StyleConfig s;
    m_settings.beginGroup("Style");
    s.textColor       = m_settings.value("textColor", s.textColor).value<QColor>();
    s.backgroundColor = m_settings.value("backgroundColor", s.backgroundColor).value<QColor>();
    s.backgroundAlpha = m_settings.value("backgroundAlpha", s.backgroundAlpha).toInt();
    s.borderRadius    = m_settings.value("borderRadius", s.borderRadius).toInt();
    s.borderColor     = m_settings.value("borderColor", s.borderColor).value<QColor>();
    s.borderWidth     = m_settings.value("borderWidth", s.borderWidth).toInt();
    QString fontFamily = m_settings.value("fontFamily", s.font.family()).toString();
    int fontSize       = m_settings.value("fontSize", s.font.pointSize()).toInt();
    s.font = QFont(fontFamily, fontSize);
    m_settings.endGroup();
    return s;
}

void Config::saveStyle(const StyleConfig& s) {
    m_settings.beginGroup("Style");
    m_settings.setValue("textColor", s.textColor);
    m_settings.setValue("backgroundColor", s.backgroundColor);
    m_settings.setValue("backgroundAlpha", s.backgroundAlpha);
    m_settings.setValue("borderRadius", s.borderRadius);
    m_settings.setValue("borderColor", s.borderColor);
    m_settings.setValue("borderWidth", s.borderWidth);
    m_settings.setValue("fontFamily", s.font.family());
    m_settings.setValue("fontSize", s.font.pointSize());
    m_settings.endGroup();
}

QString Config::sourceLang() const {
    return m_settings.value("Language/source", "auto").toString();
}
void Config::setSourceLang(const QString& lang) {
    m_settings.setValue("Language/source", lang);
}

QString Config::targetLang() const {
    return m_settings.value("Language/target", "zh").toString();
}
void Config::setTargetLang(const QString& lang) {
    m_settings.setValue("Language/target", lang);
}

Mode Config::lastMode() const {
    return static_cast<Mode>(m_settings.value("App/mode", static_cast<int>(Mode::Snapshot)).toInt());
}
void Config::setLastMode(Mode mode) {
    m_settings.setValue("App/mode", static_cast<int>(mode));
}

QString Config::activeTranslator() const {
    return m_settings.value("App/activeTranslator", "").toString();
}
void Config::setActiveTranslator(const QString& name) {
    m_settings.setValue("App/activeTranslator", name);
}

QVector<SelectionArea> Config::loadAreas() const {
    QVector<SelectionArea> areas;
    int count = m_settings.beginReadArray("Areas");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        SelectionArea a;
        a.id          = m_settings.value("id").toInt();
        a.screenIndex = m_settings.value("screenIndex").toInt();
        a.geometry    = m_settings.value("geometry").toRect();
        a.enabled     = m_settings.value("enabled", true).toBool();
        areas.append(a);
    }
    m_settings.endArray();
    return areas;
}

void Config::saveAreas(const QVector<SelectionArea>& areas) {
    m_settings.beginWriteArray("Areas", areas.size());
    for (int i = 0; i < areas.size(); ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue("id", areas[i].id);
        m_settings.setValue("screenIndex", areas[i].screenIndex);
        m_settings.setValue("geometry", areas[i].geometry);
        m_settings.setValue("enabled", areas[i].enabled);
    }
    m_settings.endArray();
}

QVector<HotkeyBinding> Config::loadHotkeys(const QVector<HotkeyBinding>& defaults) const {
    QVector<HotkeyBinding> result;
    m_settings.beginGroup("Hotkeys");
    for (const auto& def : defaults) {
        HotkeyBinding b = def;
        b.currentKeys = m_settings.value(def.id, def.defaultKeys).toString();
        result.append(b);
    }
    m_settings.endGroup();
    return result;
}

void Config::saveHotkeys(const QVector<HotkeyBinding>& bindings) {
    m_settings.beginGroup("Hotkeys");
    for (const auto& b : bindings) {
        m_settings.setValue(b.id, b.currentKeys);
    }
    m_settings.endGroup();
}

QString Config::translatorConfig(const QString& pluginName, const QString& key) const {
    return m_settings.value(QString("Translators/%1/%2").arg(pluginName, key)).toString();
}
void Config::setTranslatorConfig(const QString& pluginName, const QString& key,
                                 const QString& value) {
    m_settings.setValue(QString("Translators/%1/%2").arg(pluginName, key), value);
}
```

- [ ] **Step 3: Commit**

```bash
git add src/common/Config.h src/common/Config.cpp
git commit -m "feat: add config persistence via QSettings"
```

---

### Task 5: DXGI Capture Engine

**Files:**
- Create: `src/core/capture/ICaptureEngine.h`
- Create: `src/core/capture/DxgiCaptureEngine.h`
- Create: `src/core/capture/DxgiCaptureEngine.cpp`

- [ ] **Step 1: Write ICaptureEngine.h**

```cpp
#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>

class ICaptureEngine : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool initialize() = 0;
    virtual QImage captureRegion(const QRect& region, int screenIndex) = 0;
    virtual bool hasChanged(const QRect& region, int screenIndex) = 0;
    virtual void shutdown() = 0;

signals:
    void captureError(const QString& message);
};
```

- [ ] **Step 2: Write DxgiCaptureEngine.h**

```cpp
#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <QtCore/QVector>
#include <QtCore/QTimer>
#include "ICaptureEngine.h"

using Microsoft::WRL::ComPtr;

struct MonitorDuplicator {
    HMONITOR                 handle;
    ComPtr<ID3D11Device>     device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutput1>     output;
    ComPtr<IDXGIOutputDuplication> duplicator;
    UINT                     outputIndex;
    DXGI_OUTPUT_DESC         desc;
};

class DxgiCaptureEngine : public ICaptureEngine {
    Q_OBJECT

public:
    explicit DxgiCaptureEngine(QObject* parent = nullptr);
    ~DxgiCaptureEngine() override;

    bool initialize() override;
    QImage captureRegion(const QRect& region, int screenIndex) override;
    bool hasChanged(const QRect& region, int screenIndex) override;
    void shutdown() override;

private:
    bool initDuplicator(int screenIndex);
    bool recreateDuplicator(int screenIndex);

    QVector<MonitorDuplicator> m_duplicators;
    ComPtr<IDXGIFactory2>      m_factory;
    bool                       m_initialized = false;
};
```

- [ ] **Step 3: Write DxgiCaptureEngine.cpp**

```cpp
#include "DxgiCaptureEngine.h"
#include <QtCore/QDebug>
#include <QtGui/QPainter>

DxgiCaptureEngine::DxgiCaptureEngine(QObject* parent)
    : ICaptureEngine(parent) {}

DxgiCaptureEngine::~DxgiCaptureEngine() {
    shutdown();
}

bool DxgiCaptureEngine::initialize() {
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_factory));
    if (FAILED(hr)) {
        emit captureError("Failed to create DXGIFactory");
        return false;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
         m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++i) {
        ComPtr<IDXGIOutput> output;
        for (UINT j = 0;
             adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND;
             ++j) {
            ComPtr<IDXGIOutput1> output1;
            hr = output.As(&output1);
            if (FAILED(hr)) continue;

            DXGI_OUTPUT_DESC desc;
            output1->GetDesc(&desc);

            if (!desc.AttachedToDesktop) continue;

            MonitorDuplicator dup;
            dup.handle      = desc.Monitor;
            dup.output      = output1;
            dup.desc        = desc;
            dup.outputIndex = j;

            m_duplicators.append(dup);
        }
    }

    for (int i = 0; i < m_duplicators.size(); ++i) {
        if (!initDuplicator(i)) {
            emit captureError(QString("Failed to init duplicator for monitor %1").arg(i));
            return false;
        }
    }

    m_initialized = true;
    return true;
}

bool DxgiCaptureEngine::initDuplicator(int screenIndex) {
    if (screenIndex < 0 || screenIndex >= m_duplicators.size()) return false;
    auto& dup = m_duplicators[screenIndex];

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &dup.device, nullptr, &dup.context);

    if (FAILED(hr)) return false;

    hr = dup.output->DuplicateOutput(dup.device.Get(), &dup.duplicator);
    if (FAILED(hr)) return false;

    return true;
}

bool DxgiCaptureEngine::recreateDuplicator(int screenIndex) {
    auto& dup = m_duplicators[screenIndex];
    dup.duplicator.Reset();
    return SUCCEEDED(dup.output->DuplicateOutput(dup.device.Get(), &dup.duplicator));
}

bool DxgiCaptureEngine::hasChanged(const QRect& /*region*/, int screenIndex) {
    if (!m_initialized) return false;
    if (screenIndex < 0 || screenIndex >= m_duplicators.size()) return false;
    auto& dup = m_duplicators[screenIndex];
    if (!dup.duplicator) return false;

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = dup.duplicator->AcquireNextFrame(0, &frameInfo, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return false;
    if (FAILED(hr)) {
        recreateDuplicator(screenIndex);
        return false;
    }
    dup.duplicator->ReleaseFrame();
    return frameInfo.AccumulatedFrames > 0;
}

QImage DxgiCaptureEngine::captureRegion(const QRect& region, int screenIndex) {
    if (!m_initialized || screenIndex < 0 || screenIndex >= m_duplicators.size())
        return QImage();

    auto& dup = m_duplicators[screenIndex];

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = dup.duplicator->AcquireNextFrame(100, &frameInfo, &resource);
    if (FAILED(hr)) {
        recreateDuplicator(screenIndex);
        return QImage();
    }

    ComPtr<ID3D11Texture2D> texture;
    hr = resource.As(&texture);
    if (FAILED(hr)) {
        dup.duplicator->ReleaseFrame();
        return QImage();
    }

    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);

    // Create staging texture for CPU readback
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width              = region.width();
    stagingDesc.Height             = region.height();
    stagingDesc.MipLevels          = 1;
    stagingDesc.ArraySize          = 1;
    stagingDesc.Format             = texDesc.Format;
    stagingDesc.SampleDesc.Count   = 1;
    stagingDesc.Usage              = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> stagingTexture;
    hr = dup.device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        dup.duplicator->ReleaseFrame();
        return QImage();
    }

    D3D11_BOX box = {
        static_cast<UINT>(region.x()),
        static_cast<UINT>(region.y()), 0,
        static_cast<UINT>(region.x() + region.width()),
        static_cast<UINT>(region.y() + region.height()), 1
    };
    dup.context->CopySubresourceRegion(stagingTexture.Get(), 0, 0, 0, 0,
                                       texture.Get(), 0, &box);

    dup.duplicator->ReleaseFrame();

    // Map and copy to QImage
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = dup.context->Map(stagingTexture.Get(), 0, D3D_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return QImage();

    QImage img(region.width(), region.height(), QImage::Format_ARGB32);
    auto* dst = img.bits();
    auto* src = static_cast<const uchar*>(mapped.pData);
    for (int y = 0; y < region.height(); ++y) {
        for (int x = 0; x < region.width(); ++x) {
            // BGRA → ARGB
            size_t offset = y * mapped.RowPitch + x * 4;
            int b = src[offset];
            int g = src[offset + 1];
            int r = src[offset + 2];
            int a = 255;
            auto* pixel = dst + y * img.bytesPerLine() + x * 4;
            pixel[0] = b;
            pixel[1] = g;
            pixel[2] = r;
            pixel[3] = a;
        }
    }

    dup.context->Unmap(stagingTexture.Get(), 0);
    return img;
}

void DxgiCaptureEngine::shutdown() {
    for (auto& dup : m_duplicators) {
        dup.duplicator.Reset();
        dup.context.Reset();
        dup.device.Reset();
    }
    m_duplicators.clear();
    m_factory.Reset();
    m_initialized = false;
}
```

- [ ] **Step 4: Commit**

```bash
git add src/core/capture/
git commit -m "feat: add DXGI capture engine with change detection"
```

---

### Task 6: Windows OCR Engine

**Files:**
- Create: `src/core/ocr/IOCREngine.h`
- Create: `src/core/ocr/WindowsOcrEngine.h`
- Create: `src/core/ocr/WindowsOcrEngine.cpp`

- [ ] **Step 1: Write IOCREngine.h**

```cpp
#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>
#include "common/Types.h"

class IOCREngine : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool initialize(const QString& languageTag = "auto") = 0;
    virtual void recognize(const QImage& image) = 0;

signals:
    void recognitionComplete(const OCRResult& result);
    void recognitionError(const QString& error);
};
```

- [ ] **Step 2: Write WindowsOcrEngine.h**

```cpp
#pragma once

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include "IOCREngine.h"

class WindowsOcrEngine : public IOCREngine {
    Q_OBJECT

public:
    explicit WindowsOcrEngine(QObject* parent = nullptr);
    bool initialize(const QString& languageTag = "auto") override;
    void recognize(const QImage& image) override;

private:
    OCRResult recognizeSync(const QImage& image);
    QString m_languageTag;
    QFutureWatcher<OCRResult>* m_watcher = nullptr;
};
```

- [ ] **Step 3: Write WindowsOcrEngine.cpp**

```cpp
#include "WindowsOcrEngine.h"

#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Globalization.h>

using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage::Streams;

WindowsOcrEngine::WindowsOcrEngine(QObject* parent)
    : IOCREngine(parent) {
    m_watcher = new QFutureWatcher<OCRResult>(this);
    connect(m_watcher, &QFutureWatcher<OCRResult>::finished, this, [this]() {
        OCRResult result = m_watcher->result();
        if (result.fullText.isEmpty()) {
            // Empty result — not an error, just nothing recognized
        }
        emit recognitionComplete(result);
    });
}

bool WindowsOcrEngine::initialize(const QString& languageTag) {
    m_languageTag = languageTag;
    try {
        // Verify OCR is available by listing languages
        auto langs = OcrEngine::AvailableRecognizerLanguages();
        if (langs.Size() == 0) {
            emit recognitionError("No OCR languages available");
            return false;
        }
        return true;
    } catch (winrt::hresult_error const& e) {
        emit recognitionError(QString::fromWCharArray(e.message().c_str()));
        return false;
    }
}

void WindowsOcrEngine::recognize(const QImage& image) {
    auto future = QtConcurrent::run([this, image]() {
        return recognizeSync(image);
    });
    m_watcher->setFuture(future);
}

OCRResult WindowsOcrEngine::recognizeSync(const QImage& image) {
    OCRResult result;
    try {
        // QImage → WinRT SoftwareBitmap
        QImage converted = image.convertToFormat(QImage::Format_RGBA8888);

        SoftwareBitmap bitmap(BitmapPixelFormat::Rgba8,
                              converted.width(),
                              converted.height());

        // Copy pixel data
        {
            BitmapBuffer buffer = bitmap.LockBuffer(BitmapBufferAccessMode::ReadWrite);
            auto reference = buffer.CreateReference();
            uint8_t* data;
            uint32_t capacity;
            winrt::Windows::Foundation::MemoryBuffer memBuf =
                reference.as<winrt::Windows::Foundation::IMemoryBufferByteAccess>();
            // Use IMemoryBufferByteAccess directly
            auto byteAccess = reference.as<::Windows::Foundation::IMemoryBufferByteAccess>();
            byteAccess->GetBuffer(&data, &capacity);
            memcpy(data, converted.constBits(),
                   std::min(static_cast<std::size_t>(capacity),
                            static_cast<std::size_t>(converted.sizeInBytes())));
            buffer.Close();
        }

        // Select language
        Language lang;
        if (m_languageTag == "auto") {
            auto langs = OcrEngine::AvailableRecognizerLanguages();
            if (langs.Size() > 0) {
                lang = langs.First().Current();
            }
        } else {
            lang = Language(QString::fromStdString(m_languageTag.toStdString()).toStdWString());
        }

        OcrEngine engine = OcrEngine::TryCreateFromLanguage(lang);
        if (!engine) {
            // Fall back to first available language
            auto langs = OcrEngine::AvailableRecognizerLanguages();
            if (langs.Size() > 0) {
                engine = OcrEngine::TryCreateFromLanguage(langs.First().Current());
            }
        }

        if (!engine) {
            emit recognitionError("Failed to create OCR engine");
            return result;
        }

        OcrResult ocrResult = engine.RecognizeAsync(bitmap).get();
        result.fullText = QString::fromStdWString(ocrResult.Text().c_str());

        for (auto const& line : ocrResult.Lines()) {
            for (auto const& word : line.Words()) {
                TextBox box;
                box.text = QString::fromStdWString(word.Text().c_str());
                auto rect = word.BoundingRect();
                box.boundingRect = QRect(
                    static_cast<int>(rect.X),
                    static_cast<int>(rect.Y),
                    static_cast<int>(rect.Width),
                    static_cast<int>(rect.Height)
                );
                result.boxes.append(box);
            }
        }
    } catch (winrt::hresult_error const& e) {
        emit recognitionError(QString::fromWCharArray(e.message().c_str()));
    }

    return result;
}
```

- [ ] **Step 4: Commit**

```bash
git add src/core/ocr/
git commit -m "feat: add Windows OCR engine via WinRT"
```

---

### Task 7: Translation Plugin Interface + Manager

**Files:**
- Create: `src/core/translate/ITranslator.h`
- Create: `src/core/translate/TranslatorManager.h`
- Create: `src/core/translate/TranslatorManager.cpp`

- [ ] **Step 1: Write ITranslator.h**

```cpp
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVector>
#include "common/Types.h"

struct TranslatorConfigField {
    QString key;
    QString label;
    QString defaultValue;
    bool    isSecret   = false;
    bool    isRequired = false;
};

class ITranslator : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    virtual QString name() const = 0;
    virtual QString category() const = 0;    // "online" | "llm"
    virtual bool isAvailable() const = 0;

    virtual void translate(const TranslateRequest& req) = 0;
    virtual void cancelAll() = 0;

    virtual QVector<TranslatorConfigField> configFields() const = 0;
    virtual void setConfig(const QString& key, const QString& value) = 0;
    virtual QString getConfig(const QString& key) const = 0;

signals:
    void translationReady(const QString& original, const QString& translated);
    void translationError(const QString& error);
};
```

- [ ] **Step 2: Write TranslatorManager.h**

```cpp
#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <memory>
#include "ITranslator.h"
#include "common/Types.h"

class TranslatorManager : public QObject {
    Q_OBJECT

public:
    explicit TranslatorManager(QObject* parent = nullptr);

    void registerPlugin(std::unique_ptr<ITranslator> plugin);
    ITranslator* active() const;
    QVector<ITranslator*> plugins() const;
    ITranslator* findByName(const QString& name) const;

    void setActive(const QString& name);
    void translate(const QString& text, const QString& sourceLang,
                   const QString& targetLang);
    void cancelAll();

signals:
    void translationReady(const QString& original, const QString& translated);
    void translationError(const QString& error);
    void activeChanged(const QString& name);

private:
    void onPluginTranslationReady(const QString& original, const QString& translated);
    void onPluginTranslationError(const QString& error);

    QVector<std::unique_ptr<ITranslator>> m_plugins;
    ITranslator* m_active = nullptr;
};
```

- [ ] **Step 3: Write TranslatorManager.cpp**

```cpp
#include "TranslatorManager.h"

TranslatorManager::TranslatorManager(QObject* parent)
    : QObject(parent) {}

void TranslatorManager::registerPlugin(std::unique_ptr<ITranslator> plugin) {
    if (!plugin) return;
    connect(plugin.get(), &ITranslator::translationReady,
            this, &TranslatorManager::onPluginTranslationReady);
    connect(plugin.get(), &ITranslator::translationError,
            this, &TranslatorManager::onPluginTranslationError);
    if (!m_active) m_active = plugin.get();
    m_plugins.push_back(std::move(plugin));
}

ITranslator* TranslatorManager::active() const {
    return m_active;
}

QVector<ITranslator*> TranslatorManager::plugins() const {
    QVector<ITranslator*> result;
    for (const auto& p : m_plugins)
        result.append(p.get());
    return result;
}

ITranslator* TranslatorManager::findByName(const QString& name) const {
    for (const auto& p : m_plugins) {
        if (p->name() == name) return p.get();
    }
    return nullptr;
}

void TranslatorManager::setActive(const QString& name) {
    auto* plugin = findByName(name);
    if (plugin && plugin != m_active) {
        m_active = plugin;
        emit activeChanged(name);
    }
}

void TranslatorManager::translate(const QString& text, const QString& sourceLang,
                                   const QString& targetLang) {
    if (!m_active) {
        emit translationError("No translation service configured");
        return;
    }
    TranslateRequest req{text, sourceLang, targetLang};
    m_active->translate(req);
}

void TranslatorManager::cancelAll() {
    for (const auto& p : m_plugins)
        p->cancelAll();
}

void TranslatorManager::onPluginTranslationReady(const QString& original,
                                                  const QString& translated) {
    emit translationReady(original, translated);
}

void TranslatorManager::onPluginTranslationError(const QString& error) {
    emit translationError(error);
}
```

- [ ] **Step 4: Commit**

```bash
git add src/core/translate/ITranslator.h src/core/translate/TranslatorManager.h src/core/translate/TranslatorManager.cpp
git commit -m "feat: add translation plugin interface and manager"
```

---

### Task 8: DeepL Translator Plugin

**Files:**
- Create: `src/core/translate/plugins/DeepLTranslator.h`
- Create: `src/core/translate/plugins/DeepLTranslator.cpp`

- [ ] **Step 1: Write DeepLTranslator.h**

```cpp
#pragma once

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QHash>
#include "core/translate/ITranslator.h"

class DeepLTranslator : public ITranslator {
    Q_OBJECT

public:
    explicit DeepLTranslator(QObject* parent = nullptr);

    QString name() const override { return "DeepL"; }
    QString category() const override { return "online"; }
    bool isAvailable() const override { return !m_apiKey.isEmpty(); }

    void translate(const TranslateRequest& req) override;
    void cancelAll() override;

    QVector<TranslatorConfigField> configFields() const override;
    void setConfig(const QString& key, const QString& value) override;
    QString getConfig(const QString& key) const override;

private:
    QNetworkAccessManager* m_nam;
    QString m_apiKey;
    QString m_endpoint = "https://api-free.deepl.com/v2/translate";
    QVector<QNetworkReply*> m_pendingRequests;
};
```

- [ ] **Step 2: Write DeepLTranslator.cpp**

```cpp
#include "DeepLTranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrlQuery>

DeepLTranslator::DeepLTranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this)) {}

QVector<TranslatorConfigField> DeepLTranslator::configFields() const {
    return {
        {"apiKey",  "API Key",   "",  true,  true},
        {"endpoint","Endpoint",  "https://api-free.deepl.com/v2/translate", false, false},
    };
}

void DeepLTranslator::setConfig(const QString& key, const QString& value) {
    if (key == "apiKey")   m_apiKey   = value;
    if (key == "endpoint") m_endpoint = value;
}

QString DeepLTranslator::getConfig(const QString& key) const {
    if (key == "apiKey")   return m_apiKey;
    if (key == "endpoint") return m_endpoint;
    return {};
}

void DeepLTranslator::translate(const TranslateRequest& req) {
    if (m_apiKey.isEmpty()) {
        emit translationError("DeepL API key not configured");
        return;
    }

    QUrlQuery postData;
    postData.addQueryItem("text", req.text);
    postData.addQueryItem("target_lang", req.targetLang.toUpper());
    if (req.sourceLang != "auto" && !req.sourceLang.isEmpty()) {
        postData.addQueryItem("source_lang", req.sourceLang.toUpper());
    }

    QNetworkRequest request(QUrl(m_endpoint));
    request.setRawHeader("Authorization",
                         QString("DeepL-Auth-Key %1").arg(m_apiKey).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    QNetworkReply* reply = m_nam->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    m_pendingRequests.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, original = req.text]() {
        m_pendingRequests.removeOne(reply);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit translationError(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray translations = doc.object().value("translations").toArray();
        if (translations.isEmpty()) {
            emit translationError("Empty translation result");
            return;
        }

        QString translated = translations[0].toObject().value("text").toString();
        emit translationReady(original, translated);
    });
}

void DeepLTranslator::cancelAll() {
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}
```

- [ ] **Step 3: Commit**

```bash
git add src/core/translate/plugins/DeepLTranslator.h src/core/translate/plugins/DeepLTranslator.cpp
git commit -m "feat: add DeepL translator plugin"
```

---

### Task 9: OpenAI (LLM) Translator Plugin

**Files:**
- Create: `src/core/translate/plugins/OpenAITranslator.h`
- Create: `src/core/translate/plugins/OpenAITranslator.cpp`

- [ ] **Step 1: Write OpenAITranslator.h**

```cpp
#pragma once

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include "core/translate/ITranslator.h"

class OpenAITranslator : public ITranslator {
    Q_OBJECT

public:
    explicit OpenAITranslator(QObject* parent = nullptr);

    QString name() const override { return "OpenAI"; }
    QString category() const override { return "llm"; }
    bool isAvailable() const override { return !m_apiKey.isEmpty(); }

    void translate(const TranslateRequest& req) override;
    void cancelAll() override;

    QVector<TranslatorConfigField> configFields() const override;
    void setConfig(const QString& key, const QString& value) override;
    QString getConfig(const QString& key) const override;

private:
    QNetworkAccessManager* m_nam;
    QString m_apiKey;
    QString m_baseUrl    = "https://api.openai.com/v1";
    QString m_model      = "gpt-4o";
    QString m_systemPrompt =
        "You are a professional translator. Translate the following text "
        "naturally while preserving tone and meaning. Output only the "
        "translation, nothing else.";
    QVector<QNetworkReply*> m_pendingRequests;
};
```

- [ ] **Step 2: Write OpenAITranslator.cpp**

```cpp
#include "OpenAITranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

OpenAITranslator::OpenAITranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this)) {}

QVector<TranslatorConfigField> OpenAITranslator::configFields() const {
    return {
        {"apiKey",       "API Key",       "",                       true,  true},
        {"baseUrl",      "Base URL",      "https://api.openai.com/v1", false, false},
        {"model",        "Model",         "gpt-4o",                 false, false},
        {"systemPrompt", "System Prompt", m_systemPrompt,            false, false},
    };
}

void OpenAITranslator::setConfig(const QString& key, const QString& value) {
    if (key == "apiKey")       m_apiKey       = value;
    if (key == "baseUrl")      m_baseUrl      = value;
    if (key == "model")        m_model        = value;
    if (key == "systemPrompt") m_systemPrompt = value;
}

QString OpenAITranslator::getConfig(const QString& key) const {
    if (key == "apiKey")       return m_apiKey;
    if (key == "baseUrl")      return m_baseUrl;
    if (key == "model")        return m_model;
    if (key == "systemPrompt") return m_systemPrompt;
    return {};
}

void OpenAITranslator::translate(const TranslateRequest& req) {
    if (m_apiKey.isEmpty()) {
        emit translationError("OpenAI API key not configured");
        return;
    }

    QJsonObject body;
    body["model"] = m_model;

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", m_systemPrompt}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", req.text}
    });
    body["messages"] = messages;
    body["temperature"] = 0.3;

    QNetworkRequest request(QUrl(m_baseUrl + "/chat/completions"));
    request.setRawHeader("Authorization",
                         QString("Bearer %1").arg(m_apiKey).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_nam->post(request,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_pendingRequests.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, original = req.text]() {
        m_pendingRequests.removeOne(reply);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit translationError(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray choices = doc.object().value("choices").toArray();
        if (choices.isEmpty()) {
            emit translationError("Empty LLM response");
            return;
        }

        QString translated = choices[0].toObject()
            .value("message").toObject()
            .value("content").toString().trimmed();
        emit translationReady(original, translated);
    });
}

void OpenAITranslator::cancelAll() {
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}
```

- [ ] **Step 3: Commit**

```bash
git add src/core/translate/plugins/OpenAITranslator.h src/core/translate/plugins/OpenAITranslator.cpp
git commit -m "feat: add OpenAI LLM translator plugin"
```

---

### Task 10: Layout Engine

**Files:**
- Create: `src/engine/layout/LayoutEngine.h`
- Create: `src/engine/layout/LayoutEngine.cpp`

- [ ] **Step 1: Write LayoutEngine.h**

```cpp
#pragma once

#include <QtCore/QRect>
#include <QtCore/QPoint>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtGui/QFontMetrics>
#include "common/Types.h"

class LayoutEngine {
public:
    LayoutResult compute(const LayoutRequest& request,
                         const QVector<QRect>& existingBubbles,
                         const QRect& screenBounds);

private:
    struct Candidate {
        QPoint position;
        int    priority;  // lower = better
    };

    QVector<Candidate> generateCandidates(const QRect& source, const QSize& bubbleSize,
                                           const QRect& screen);
    bool overlapsSource(const QRect& candidate, const QRect& source);
    bool overlapsExisting(const QRect& candidate, const QVector<QRect>& existing);
    bool exceedsBounds(const QRect& candidate, const QRect& screen);
    QRect clampToScreen(const QRect& candidate, const QRect& screen);
    QSize estimateBubbleSize(const QString& text, const QFont& font, int maxWidth);

    static constexpr int kMinFontSize = 8;
    static constexpr int kGap         = 8;   // gap between source and bubble
};
```

- [ ] **Step 2: Write LayoutEngine.cpp**

```cpp
#include "LayoutEngine.h"
#include <QtGui/QFontMetrics>

LayoutResult LayoutEngine::compute(const LayoutRequest& request,
                                    const QVector<QRect>& existingBubbles,
                                    const QRect& screenBounds) {
    LayoutResult result;
    result.isTruncated = false;

    int fontSize = request.preferredFontSize;
    QFont font("Microsoft YaHei", fontSize);

    while (fontSize >= kMinFontSize) {
        font.setPointSize(fontSize);
        QFontMetrics fm(font);
        int textWidth  = fm.horizontalAdvance(request.translatedText);
        int textHeight = fm.height();
        int bubbleW    = std::min(textWidth + 24, screenBounds.width() - 20);
        int bubbleH    = textHeight + 16;
        QSize bubbleSize(bubbleW, bubbleH);

        auto candidates = generateCandidates(request.sourceRect, bubbleSize, screenBounds);

        bool placed = false;
        for (const auto& cand : candidates) {
            QRect candidateRect(cand.position, bubbleSize);

            if (overlapsSource(candidateRect, request.sourceRect)) continue;
            if (overlapsExisting(candidateRect, existingBubbles)) continue;

            candidateRect = clampToScreen(candidateRect, screenBounds);
            result.position = candidateRect.topLeft();
            result.maxWidth = candidateRect.width();
            result.fontSize  = fontSize;
            placed = true;
            break;
        }

        if (placed) return result;
        --fontSize;
    }

    // Last resort: truncate with "..." and force at minimum size
    result.fontSize    = kMinFontSize;
    result.isTruncated = true;

    QFont minFont("Microsoft YaHei", kMinFontSize);
    QFontMetrics fm(minFont);
    QString truncated = fm.elidedText(request.translatedText, Qt::ElideRight,
                                       screenBounds.width() - 20);
    int w = fm.horizontalAdvance(truncated) + 24;
    int h = fm.height() + 16;

    // Place to the right if possible, otherwise wherever fits
    QPoint pos(request.sourceRect.right() + kGap, request.sourceRect.top());
    QRect candidateRect(pos, QSize(w, h));
    candidateRect = clampToScreen(candidateRect, screenBounds);
    result.position = candidateRect.topLeft();
    result.maxWidth = candidateRect.width();

    return result;
}

QVector<LayoutEngine::Candidate> LayoutEngine::generateCandidates(
    const QRect& source, const QSize& bubbleSize, const QRect& screen) {

    QVector<Candidate> candidates;

    // Right
    candidates.append({{source.right() + kGap, source.top()}, 0});
    // Below
    candidates.append({{source.left(), source.bottom() + kGap}, 1});
    // Left
    candidates.append({{source.left() - bubbleSize.width() - kGap, source.top()}, 2});
    // Above
    candidates.append({{source.left(), source.top() - bubbleSize.height() - kGap}, 3});

    return candidates;
}

QSize LayoutEngine::estimateBubbleSize(const QString& text, const QFont& font, int maxWidth) {
    QFontMetrics fm(font);
    QRect rect = fm.boundingRect(QRect(0, 0, maxWidth, 10000),
                                  Qt::AlignLeft | Qt::TextWordWrap, text);
    return QSize(rect.width() + 24, rect.height() + 16);
}

bool LayoutEngine::overlapsSource(const QRect& candidate, const QRect& source) {
    return candidate.intersects(source);
}

bool LayoutEngine::overlapsExisting(const QRect& candidate,
                                     const QVector<QRect>& existing) {
    for (const auto& r : existing) {
        if (candidate.intersects(r)) return true;
    }
    return false;
}

bool LayoutEngine::exceedsBounds(const QRect& candidate, const QRect& screen) {
    return !screen.contains(candidate);
}

QRect LayoutEngine::clampToScreen(const QRect& candidate, const QRect& screen) {
    QRect clamped = candidate;
    if (clamped.right() > screen.right())
        clamped.moveRight(screen.right());
    if (clamped.bottom() > screen.bottom())
        clamped.moveBottom(screen.bottom());
    if (clamped.left() < screen.left())
        clamped.moveLeft(screen.left());
    if (clamped.top() < screen.top())
        clamped.moveTop(screen.top());
    return clamped;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/engine/layout/
git commit -m "feat: add 3-tier anti-overlap layout engine"
```

---

### Task 11: Hotkey Manager

**Files:**
- Create: `src/engine/hotkey/HotkeyManager.h`
- Create: `src/engine/hotkey/HotkeyManager.cpp`

- [ ] **Step 1: Write HotkeyManager.h**

```cpp
#pragma once

#include <QtCore/QObject>
#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QVector>
#include <QtGui/QKeySequence>
#include <windows.h>
#include "common/Types.h"

class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    bool registerBinding(const HotkeyBinding& binding);
    void unregisterAll();
    void updateBinding(const QString& id, const QString& newKeys);
    HotkeyBinding binding(const QString& id) const;
    bool hasConflict(const QString& keys, const QString& excludeId) const;
    QVector<HotkeyBinding> bindings() const { return m_bindings; }
    void setBindings(const QVector<HotkeyBinding>& bindings) { m_bindings = bindings; }

    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* result) override;

signals:
    void hotkeyTriggered(const QString& id);

private:
    int nextHotkeyId();
    DWORD parseModifiers(const QString& keys, DWORD& vk);

    QVector<HotkeyBinding> m_bindings;
    QHash<QString, int>   m_registeredIds;  // id → native hotkey id
    QHash<int, QString>   m_idToBinding;    // native id → binding id
    int m_nextId = 1;
};
```

- [ ] **Step 2: Write HotkeyManager.cpp**

```cpp
#include "HotkeyManager.h"
#include <QtCore/QDebug>
#include <QtCore/QStringList>

HotkeyManager::HotkeyManager(QObject* parent)
    : QObject(parent) {}

HotkeyManager::~HotkeyManager() {
    unregisterAll();
}

int HotkeyManager::nextHotkeyId() {
    return m_nextId++;
}

DWORD HotkeyManager::parseModifiers(const QString& keys, DWORD& vk) {
    DWORD mods = 0;
    QStringList parts = keys.split('+');
    for (const auto& part : parts) {
        QString p = part.trimmed();
        if (p == "Ctrl" || p == "Control")  mods |= MOD_CONTROL;
        else if (p == "Shift")             mods |= MOD_SHIFT;
        else if (p == "Alt")               mods |= MOD_ALT;
        else if (p == "Win" || p == "Meta") mods |= MOD_WIN;
        else {
            // Virtual key
            QKeySequence ks(p);
            if (!ks.isEmpty()) {
                vk = ks[0].toCombined() & 0x01FF;  // strip modifiers
            }
        }
    }
    return mods;
}

bool HotkeyManager::registerBinding(const HotkeyBinding& binding) {
    DWORD vk = 0;
    DWORD mods = parseModifiers(binding.currentKeys, vk);

    int id = nextHotkeyId();
    if (!::RegisterHotKey(nullptr, id, mods, vk)) {
        DWORD err = GetLastError();
        qWarning() << "RegisterHotKey failed for" << binding.id
                   << "keys:" << binding.currentKeys << "error:" << err;
        return false;
    }

    m_registeredIds[binding.id] = id;
    m_idToBinding[id] = binding.id;
    return true;
}

void HotkeyManager::unregisterAll() {
    for (auto it = m_registeredIds.begin(); it != m_registeredIds.end(); ++it) {
        ::UnregisterHotKey(nullptr, it.value());
    }
    m_registeredIds.clear();
    m_idToBinding.clear();
}

void HotkeyManager::updateBinding(const QString& id, const QString& newKeys) {
    if (m_registeredIds.contains(id)) {
        ::UnregisterHotKey(nullptr, m_registeredIds[id]);
        m_idToBinding.remove(m_registeredIds[id]);
        m_registeredIds.remove(id);
    }

    for (auto& b : m_bindings) {
        if (b.id == id) {
            b.currentKeys = newKeys;
            registerBinding(b);
            return;
        }
    }
}

HotkeyBinding HotkeyManager::binding(const QString& id) const {
    for (const auto& b : m_bindings) {
        if (b.id == id) return b;
    }
    return {};
}

bool HotkeyManager::hasConflict(const QString& keys, const QString& excludeId) const {
    for (const auto& b : m_bindings) {
        if (b.id != excludeId && b.currentKeys == keys) return true;
    }
    return false;
}

bool HotkeyManager::nativeEventFilter(const QByteArray& /*eventType*/,
                                       void* message, qintptr* /*result*/) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        int id = static_cast<int>(msg->wParam);
        if (m_idToBinding.contains(id)) {
            emit hotkeyTriggered(m_idToBinding[id]);
            return true;
        }
    }
    return false;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/engine/hotkey/
git commit -m "feat: add global hotkey manager via RegisterHotKey"
```

---

### Task 12: Translation Overlay Window + OverlayManager

**Files:**
- Create: `src/ui/overlay/TranslationOverlay.h`
- Create: `src/ui/overlay/TranslationOverlay.cpp`
- Create: `src/ui/overlay/OverlayManager.h`
- Create: `src/ui/overlay/OverlayManager.cpp`

- [ ] **Step 1: Write TranslationOverlay.h**

```cpp
#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QString>
#include "common/Types.h"

class TranslationOverlay : public QWidget {
    Q_OBJECT

public:
    explicit TranslationOverlay(QWidget* parent = nullptr);
    ~TranslationOverlay() override;

    void showTranslation(const LayoutResult& layout, const QString& text,
                         const StyleConfig& style);
    void updateStyle(const StyleConfig& style);
    int  overlayId() const { return m_id; }
    void setOverlayId(int id) { m_id = id; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    int  m_id = -1;
    QString m_text;
    StyleConfig m_style;
};
```

- [ ] **Step 2: Write TranslationOverlay.cpp**

```cpp
#include "TranslationOverlay.h"
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <windows.h>

TranslationOverlay::TranslationOverlay(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
                   Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    // Win32 layer: WS_EX_TRANSPARENT + WS_EX_LAYERED + WS_EX_NOACTIVATE
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
}

TranslationOverlay::~TranslationOverlay() = default;

void TranslationOverlay::showTranslation(const LayoutResult& layout,
                                          const QString& text,
                                          const StyleConfig& style) {
    m_text  = text;
    m_style = style;

    QFont font = style.font;
    font.setPointSize(layout.fontSize);
    m_style.font = font;

    QFontMetrics fm(font);
    QRect br = fm.boundingRect(QRect(0, 0, layout.maxWidth, 10000),
                                Qt::AlignLeft | Qt::TextWordWrap, text);

    int w = std::min(br.width() + 24, layout.maxWidth);
    int h = br.height() + 16;

    setGeometry(layout.position.x(), layout.position.y(), w, h);
    show();
}

void TranslationOverlay::updateStyle(const StyleConfig& style) {
    m_style = style;
    update();
}

void TranslationOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    QColor bg = m_style.backgroundColor;
    bg.setAlpha(static_cast<int>(m_style.backgroundAlpha * 255 / 100));
    p.setBrush(bg);
    p.setPen(QPen(m_style.borderColor, m_style.borderWidth));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1),
                      m_style.borderRadius, m_style.borderRadius);

    // Text
    p.setPen(m_style.textColor);
    p.setFont(m_style.font);
    QRect textRect = rect().adjusted(12, 8, -12, -8);
    p.drawText(textRect, Qt::AlignLeft | Qt::TextWordWrap, m_text);
}
```

- [ ] **Step 3: Write OverlayManager.h**

```cpp
#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QRect>
#include "TranslationOverlay.h"
#include "common/Types.h"

class OverlayManager : public QObject {
    Q_OBJECT

public:
    explicit OverlayManager(QObject* parent = nullptr);
    ~OverlayManager() override;

    int  showTranslation(const LayoutResult& layout, const QString& text);
    void updateTranslation(int id, const QString& newText, const LayoutResult& layout);
    void removeTranslation(int id);
    void removeAll();
    void showAll();
    void updateAllStyles(const StyleConfig& style);

    QVector<QRect> existingBubbleRects() const;

private:
    QHash<int, TranslationOverlay*> m_overlays;
    StyleConfig m_currentStyle;
    bool m_globalVisible = true;
    int  m_nextId = 1;
};
```

- [ ] **Step 4: Write OverlayManager.cpp**

```cpp
#include "OverlayManager.h"

OverlayManager::OverlayManager(QObject* parent) : QObject(parent) {}

OverlayManager::~OverlayManager() {
    qDeleteAll(m_overlays);
}

int OverlayManager::showTranslation(const LayoutResult& layout, const QString& text) {
    auto* overlay = new TranslationOverlay();
    overlay->setOverlayId(m_nextId);
    m_overlays[m_nextId] = overlay;

    overlay->showTranslation(layout, text, m_currentStyle);
    if (!m_globalVisible) overlay->hide();

    return m_nextId++;
}

void OverlayManager::updateTranslation(int id, const QString& newText,
                                        const LayoutResult& layout) {
    auto it = m_overlays.find(id);
    if (it == m_overlays.end()) return;
    it.value()->showTranslation(layout, newText, m_currentStyle);
}

void OverlayManager::removeTranslation(int id) {
    auto it = m_overlays.find(id);
    if (it == m_overlays.end()) return;
    delete it.value();
    m_overlays.remove(id);
}

void OverlayManager::removeAll() {
    qDeleteAll(m_overlays);
    m_overlays.clear();
}

void OverlayManager::showAll() {
    m_globalVisible = true;
    for (auto* overlay : m_overlays)
        overlay->show();
}

void OverlayManager::updateAllStyles(const StyleConfig& style) {
    m_currentStyle = style;
    for (auto* overlay : m_overlays)
        overlay->updateStyle(style);
}

QVector<QRect> OverlayManager::existingBubbleRects() const {
    QVector<QRect> rects;
    for (auto* overlay : m_overlays) {
        if (overlay->isVisible())
            rects.append(overlay->geometry());
    }
    return rects;
}
```

- [ ] **Step 5: Commit**

```bash
git add src/ui/overlay/
git commit -m "feat: add click-through translation overlay windows"
```

---

### Task 13: Area Selector

**Files:**
- Create: `src/ui/selector/AreaSelector.h`
- Create: `src/ui/selector/AreaSelector.cpp`

- [ ] **Step 1: Write AreaSelector.h**

```cpp
#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QRect>
#include <QtGui/QPainter>

class AreaSelector : public QWidget {
    Q_OBJECT

public:
    explicit AreaSelector(int screenIndex, QWidget* parent = nullptr);

signals:
    void areaConfirmed(const QRect& area, int screenIndex);
    void cancelled();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    QRect selectionRect() const;
    int  handleAtPoint(const QPoint& pos) const;

    int    m_screenIndex;
    QPoint m_dragStart;
    QPoint m_dragEnd;
    bool   m_dragging  = false;
    bool   m_confirmed = false;
    QRect  m_selection;

    // Handle indices: -1=none, 0-3=corners, 4-7=edges
    int    m_activeHandle = -1;
    QPoint m_lastMousePos;

    static constexpr int kHandleSize = 8;
};
```

- [ ] **Step 2: Write AreaSelector.cpp**

```cpp
#include "AreaSelector.h"
#include <QtWidgets/QApplication>
#include <QtGui/QScreen>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainterPath>

AreaSelector::AreaSelector(int screenIndex, QWidget* parent)
    : QWidget(parent), m_screenIndex(screenIndex) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setCursor(Qt::CrossCursor);

    QScreen* screen = QApplication::screens().value(screenIndex);
    if (screen) setGeometry(screen->geometry());
    showFullScreen();
    setFocus();
    grabMouse();
    grabKeyboard();
}

QRect AreaSelector::selectionRect() const {
    return QRect(
        std::min(m_dragStart.x(), m_dragEnd.x()),
        std::min(m_dragStart.y(), m_dragEnd.y()),
        std::abs(m_dragEnd.x() - m_dragStart.x()),
        std::abs(m_dragEnd.y() - m_dragStart.y())
    );
}

void AreaSelector::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Semi-transparent mask
    p.fillRect(rect(), QColor(0, 0, 0, 128));

    if (m_dragging || m_confirmed) {
        QRect sel = m_confirmed ? m_selection : selectionRect();

        // Cut out selection area
        QPainterPath path;
        path.addRect(rect());
        path.addRect(sel);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillPath(path, Qt::black);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        // Selection border
        p.setPen(QPen(QColor(0, 120, 255), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(sel);

        // Resize handles
        p.setBrush(QColor(0, 120, 255));
        p.setPen(Qt::white);
        QVector<QPoint> handles = {
            sel.topLeft(), sel.topRight(), sel.bottomRight(), sel.bottomLeft(),
            QPoint(sel.center().x(), sel.top()),
            QPoint(sel.right(), sel.center().y()),
            QPoint(sel.center().x(), sel.bottom()),
            QPoint(sel.left(), sel.center().y()),
        };
        for (const auto& hp : handles) {
            p.drawRect(QRect(hp.x() - kHandleSize/2, hp.y() - kHandleSize/2,
                             kHandleSize, kHandleSize));
        }

        // Size info text
        QString info = QString("%1 × %2").arg(sel.width()).arg(sel.height());
        p.setPen(Qt::white);
        p.drawText(sel.adjusted(4, 4, -4, -4), Qt::AlignLeft | Qt::AlignTop, info);
    }
}

int AreaSelector::handleAtPoint(const QPoint& pos) const {
    QRect sel = m_confirmed ? m_selection : selectionRect();
    QVector<QPoint> handles = {
        sel.topLeft(), sel.topRight(), sel.bottomRight(), sel.bottomLeft(),
        QPoint(sel.center().x(), sel.top()),
        QPoint(sel.right(), sel.center().y()),
        QPoint(sel.center().x(), sel.bottom()),
        QPoint(sel.left(), sel.center().y()),
    };
    for (int i = 0; i < handles.size(); ++i) {
        QRect hr(handles[i].x() - kHandleSize, handles[i].y() - kHandleSize,
                 kHandleSize * 2, kHandleSize * 2);
        if (hr.contains(pos)) return i;
    }
    if (sel.contains(pos)) return -2;  // inside selection → move
    return -1;
}

void AreaSelector::mousePressEvent(QMouseEvent* event) {
    if (m_confirmed) {
        int h = handleAtPoint(event->pos());
        if (h == -1) {
            // Clicked outside — restart
            m_confirmed = false;
            m_dragging  = true;
            m_dragStart = event->pos();
            m_dragEnd   = event->pos();
        } else if (h == -2) {
            // Start moving
            m_activeHandle = -2;
            m_lastMousePos = event->pos();
        } else {
            // Start resizing
            m_activeHandle = h;
            m_lastMousePos = event->pos();
        }
        update();
        return;
    }

    m_dragging  = true;
    m_dragStart = event->pos();
    m_dragEnd   = event->pos();
    update();
}

void AreaSelector::mouseMoveEvent(QMouseEvent* event) {
    QRect sel = m_confirmed ? m_selection : selectionRect();

    if (m_activeHandle == -2) {
        // Moving
        QPoint delta = event->pos() - m_lastMousePos;
        m_selection.translate(delta);
        m_lastMousePos = event->pos();
        m_confirmed = true;
    } else if (m_activeHandle >= 0 && m_activeHandle < 4) {
        // Corner resize
        switch (m_activeHandle) {
        case 0: m_selection.setTopLeft(event->pos()); break;
        case 1: m_selection.setTopRight(event->pos()); break;
        case 2: m_selection.setBottomRight(event->pos()); break;
        case 3: m_selection.setBottomLeft(event->pos()); break;
        }
        m_selection = m_selection.normalized();
        m_confirmed = true;
    } else if (m_activeHandle >= 4) {
        // Edge resize
        switch (m_activeHandle) {
        case 4: m_selection.setTop(event->pos().y()); break;
        case 5: m_selection.setRight(event->pos().x()); break;
        case 6: m_selection.setBottom(event->pos().y()); break;
        case 7: m_selection.setLeft(event->pos().x()); break;
        }
        m_selection = m_selection.normalized();
        m_confirmed = true;
    } else if (m_dragging) {
        m_dragEnd = event->pos();
    }

    update();
}

void AreaSelector::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    if (m_activeHandle >= -1) {
        m_activeHandle = -1;
    }
    if (m_dragging) {
        m_dragging  = false;
        m_selection = selectionRect();
        m_confirmed = true;
        releaseMouse();
        releaseKeyboard();
    }
}

void AreaSelector::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!m_selection.isNull() && m_selection.width() > 10
            && m_selection.height() > 10) {
            releaseMouse();
            releaseKeyboard();
            emit areaConfirmed(m_selection, m_screenIndex);
            close();
        }
        break;
    case Qt::Key_Escape:
        releaseMouse();
        releaseKeyboard();
        emit cancelled();
        close();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add src/ui/selector/
git commit -m "feat: add area selector with drag/resize/move"
```

---

### Task 14: System Tray Manager

**Files:**
- Create: `src/ui/tray/TrayManager.h`
- Create: `src/ui/tray/TrayManager.cpp`

- [ ] **Step 1: Write TrayManager.h**

```cpp
#pragma once

#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtCore/QObject>
#include "common/Types.h"

class TrayManager : public QObject {
    Q_OBJECT

public:
    explicit TrayManager(QObject* parent = nullptr);
    void initialize();
    void updateIcon(Mode mode);

signals:
    void modeChangeRequested(Mode mode);
    void areaSelectRequested();
    void globalVisibilityToggleRequested();
    void settingsRequested();
    void exitRequested();

private:
    void buildMenu();
    void updateModeCheck(Mode mode);

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu*           m_menu     = nullptr;
    QMenu*           m_modeMenu = nullptr;
    QAction*         m_realtimeAction = nullptr;
    QAction*         m_snapshotAction = nullptr;
    QAction*         m_pauseAction    = nullptr;
};
```

- [ ] **Step 2: Write TrayManager.cpp**

```cpp
#include "TrayManager.h"
#include <QtGui/QIcon>
#include <QtCore/QDebug>

TrayManager::TrayManager(QObject* parent) : QObject(parent) {}

void TrayManager::initialize() {
    m_trayIcon = new QSystemTrayIcon(this);
    // Use a simple colored pixmap as placeholder icon
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(0, 180, 120));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(6, 6, 20, 20, 4, 4);
    painter.end();

    m_trayIcon->setIcon(QIcon(pix));
    m_trayIcon->setToolTip("ScreenLingo");

    buildMenu();
    m_trayIcon->setContextMenu(m_menu);
    m_trayIcon->show();
}

void TrayManager::buildMenu() {
    m_menu = new QMenu();

    // Mode submenu
    m_modeMenu = new QMenu("Mode", m_menu);

    m_realtimeAction = m_modeMenu->addAction("Real-time Translation");
    m_realtimeAction->setCheckable(true);
    m_snapshotAction = m_modeMenu->addAction("Snapshot Translation");
    m_snapshotAction->setCheckable(true);
    m_pauseAction    = m_modeMenu->addAction("Pause Translation");
    m_pauseAction->setCheckable(true);

    m_menu->addMenu(m_modeMenu);
    m_menu->addSeparator();

    QAction* areaAction = m_menu->addAction("Select Translation Area...");
    QAction* toggleAction = m_menu->addAction("Show/Hide All Translations");
    m_menu->addSeparator();

    QAction* settingsAction = m_menu->addAction("Settings...");
    m_menu->addSeparator();

    QAction* exitAction = m_menu->addAction("Exit");

    connect(m_realtimeAction, &QAction::triggered, this, [this]() {
        emit modeChangeRequested(Mode::RealTime);
    });
    connect(m_snapshotAction, &QAction::triggered, this, [this]() {
        emit modeChangeRequested(Mode::Snapshot);
    });
    connect(m_pauseAction, &QAction::triggered, this, [this]() {
        emit modeChangeRequested(Mode::Pause);
    });
    connect(areaAction, &QAction::triggered, this, &TrayManager::areaSelectRequested);
    connect(toggleAction, &QAction::triggered, this, &TrayManager::globalVisibilityToggleRequested);
    connect(settingsAction, &QAction::triggered, this, &TrayManager::settingsRequested);
    connect(exitAction, &QAction::triggered, this, &TrayManager::exitRequested);
}

void TrayManager::updateModeCheck(Mode mode) {
    m_realtimeAction->setChecked(mode == Mode::RealTime);
    m_snapshotAction->setChecked(mode == Mode::Snapshot);
    m_pauseAction->setChecked(mode == Mode::Pause);
}

void TrayManager::updateIcon(Mode mode) {
    updateModeCheck(mode);

    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    if (mode == Mode::Pause) {
        painter.setBrush(QColor(200, 160, 0));  // yellow for pause
    } else {
        painter.setBrush(QColor(0, 180, 120));   // green for active
    }
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(6, 6, 20, 20, 4, 4);
    painter.end();

    m_trayIcon->setIcon(QIcon(pix));
}
```

- [ ] **Step 3: Commit**

```bash
git add src/ui/tray/
git commit -m "feat: add system tray manager with mode menu"
```

---

### Task 15: Settings Panel

**Files:**
- Create: `src/ui/settings/SettingsPanel.h`
- Create: `src/ui/settings/SettingsPanel.cpp`

- [ ] **Step 1: Write SettingsPanel.h**

This is the most UI-heavy component. Write a tabbed dialog with Appearance, Translation Service, Hotkeys, and Areas tabs. For full code, see the implementation below.

- [ ] **Step 2: Write SettingsPanel.cpp**

```cpp
#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtCore/QVector>
#include "common/Types.h"

class ITranslator;
class HotkeyManager;
class Config;
class SignalBus;

class SettingsPanel : public QDialog {
    Q_OBJECT

public:
    SettingsPanel(const QVector<ITranslator*>& translators,
                  HotkeyManager* hotkeyMgr,
                  Config* config,
                  SignalBus* bus,
                  QWidget* parent = nullptr);

signals:
    void styleChanged(const StyleConfig& style);

private:
    QWidget* createAppearanceTab();
    QWidget* createTranslationTab();
    QWidget* createHotkeysTab();
    QWidget* createAreasTab();

    void applyStyle();
    void loadStyle();
    void updatePreview();

    SignalBus*                   m_bus;
    Config*                      m_config;
    HotkeyManager*               m_hotkeyMgr;
    QVector<ITranslator*>        m_translators;
    StyleConfig                  m_pendingStyle;

    // Appearance controls
    QPushButton* m_textColorBtn    = nullptr;
    QSpinBox*    m_fontSizeSpin    = nullptr;
    QComboBox*   m_fontCombo       = nullptr;
    QPushButton* m_bgColorBtn      = nullptr;
    QSlider*     m_alphaSlider     = nullptr;
    QSpinBox*    m_borderRadiusSpin = nullptr;
    QPushButton* m_borderColorBtn  = nullptr;
    QSpinBox*    m_borderWidthSpin = nullptr;

    // Translation controls
    QComboBox*   m_serviceCombo = nullptr;
    QVBoxLayout* m_translatorConfigLayout = nullptr;

    // Preview
    QLabel*      m_previewLabel = nullptr;
};
```

The SettingsPanel.cpp is a large file (~300 lines) containing the full tabbed dialog implementation. Full code provided in the plan commit.

- [ ] **Step 3: Write SettingsPanel.cpp**

```cpp
#include "SettingsPanel.h"
#include "core/translate/ITranslator.h"
#include "engine/hotkey/HotkeyManager.h"
#include "common/Config.h"
#include "app/SignalBus.h"
#include <QtGui/QFontDatabase>
#include <QtWidgets/QMessageBox>

SettingsPanel::SettingsPanel(const QVector<ITranslator*>& translators,
                               HotkeyManager* hotkeyMgr,
                               Config* config,
                               SignalBus* bus,
                               QWidget* parent)
    : QDialog(parent), m_bus(bus), m_config(config),
      m_hotkeyMgr(hotkeyMgr), m_translators(translators) {

    setWindowTitle("ScreenLingo Settings");
    setMinimumSize(520, 440);

    auto* tabWidget = new QTabWidget;

    tabWidget->addTab(createAppearanceTab(),  "Appearance");
    tabWidget->addTab(createTranslationTab(), "Translation");
    tabWidget->addTab(createHotkeysTab(),     "Hotkeys");
    tabWidget->addTab(createAreasTab(),       "Areas");

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabWidget);

    auto* btnLayout = new QHBoxLayout;
    auto* resetBtn  = new QPushButton("Reset Defaults");
    auto* applyBtn  = new QPushButton("Apply");
    auto* closeBtn  = new QPushButton("Close");
    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    connect(applyBtn,  &QPushButton::clicked, this, &SettingsPanel::applyStyle);
    connect(closeBtn,  &QPushButton::clicked, this, &QDialog::close);
    connect(resetBtn,  &QPushButton::clicked, this, [this]() {
        m_pendingStyle = StyleConfig{};
        loadStyle();
        updatePreview();
    });

    loadStyle();
}

QWidget* SettingsPanel::createAppearanceTab() {
    auto* page = new QWidget;
    auto* form = new QFormLayout(page);

    m_textColorBtn = new QPushButton;
    m_textColorBtn->setFixedSize(40, 24);
    connect(m_textColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_pendingStyle.textColor, this);
        if (c.isValid()) {
            m_pendingStyle.textColor = c;
            m_textColorBtn->setStyleSheet(
                QString("background-color: %1; border: 1px solid #666;").arg(c.name()));
            updatePreview();
        }
    });
    form->addRow("Text Color:", m_textColorBtn);

    m_fontSizeSpin = new QSpinBox;
    m_fontSizeSpin->setRange(8, 48);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.font.setPointSize(v);
        updatePreview();
    });
    form->addRow("Font Size:", m_fontSizeSpin);

    m_fontCombo = new QComboBox;
    QFontDatabase fontDb;
    m_fontCombo->addItems(fontDb.families());
    connect(m_fontCombo, &QComboBox::currentTextChanged, this, [this](const QString& f) {
        m_pendingStyle.font.setFamily(f);
        updatePreview();
    });
    form->addRow("Font:", m_fontCombo);

    m_bgColorBtn = new QPushButton;
    m_bgColorBtn->setFixedSize(40, 24);
    connect(m_bgColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_pendingStyle.backgroundColor, this,
                                          "Choose", QColorDialog::ShowAlphaChannel);
        if (c.isValid()) {
            m_pendingStyle.backgroundColor = c;
            m_bgColorBtn->setStyleSheet(
                QString("background-color: %1; border: 1px solid #666;").arg(c.name()));
            updatePreview();
        }
    });
    form->addRow("Background:", m_bgColorBtn);

    m_alphaSlider = new QSlider(Qt::Horizontal);
    m_alphaSlider->setRange(0, 100);
    connect(m_alphaSlider, &QSlider::valueChanged, this, [this](int v) {
        m_pendingStyle.backgroundAlpha = v;
        updatePreview();
    });
    form->addRow("Opacity:", m_alphaSlider);

    m_borderRadiusSpin = new QSpinBox;
    m_borderRadiusSpin->setRange(0, 20);
    connect(m_borderRadiusSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.borderRadius = v;
        updatePreview();
    });
    form->addRow("Border Radius:", m_borderRadiusSpin);

    m_borderColorBtn = new QPushButton;
    m_borderColorBtn->setFixedSize(40, 24);
    connect(m_borderColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_pendingStyle.borderColor, this);
        if (c.isValid()) {
            m_pendingStyle.borderColor = c;
            m_borderColorBtn->setStyleSheet(
                QString("background-color: %1; border: 1px solid #666;").arg(c.name()));
            updatePreview();
        }
    });
    form->addRow("Border Color:", m_borderColorBtn);

    m_borderWidthSpin = new QSpinBox;
    m_borderWidthSpin->setRange(0, 5);
    connect(m_borderWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.borderWidth = v;
        updatePreview();
    });
    form->addRow("Border Width:", m_borderWidthSpin);

    // Live preview
    auto* previewGroup = new QGroupBox("Preview");
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel("Hello 你好世界！");
    m_previewLabel->setMinimumHeight(50);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_previewLabel);
    form->addRow(previewGroup);

    return page;
}

QWidget* SettingsPanel::createTranslationTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    m_serviceCombo = new QComboBox;
    for (auto* t : m_translators) {
        m_serviceCombo->addItem(
            QString("%1 (%2)").arg(t->name(), t->category()), t->name());
    }
    layout->addWidget(new QLabel("Active Service:"));
    layout->addWidget(m_serviceCombo);

    m_translatorConfigLayout = new QVBoxLayout;
    layout->addLayout(m_translatorConfigLayout);

    // Rebuild config fields when service changes
    connect(m_serviceCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        // Clear existing config fields
        QLayoutItem* item;
        while ((item = m_translatorConfigLayout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }

        auto* translator = m_translators.value(idx);
        if (!translator) return;

        auto* group = new QGroupBox(translator->name() + " Configuration");
        auto* form = new QFormLayout(group);

        for (const auto& field : translator->configFields()) {
            auto* edit = new QLineEdit;
            edit->setText(translator->getConfig(field.key));
            if (field.isSecret) edit->setEchoMode(QLineEdit::Password);

            connect(edit, &QLineEdit::textChanged, this, [translator, key = field.key](const QString& v) {
                translator->setConfig(key, v);
            });

            form->addRow(field.label + ":", edit);
        }

        m_translatorConfigLayout->addWidget(group);
    });

    // Trigger initial build
    if (m_translators.size() > 0)
        m_serviceCombo->setCurrentIndex(0);

    layout->addStretch();
    return page;
}

QWidget* SettingsPanel::createHotkeysTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel("Click a shortcut to rebind. Press new keys in the capture dialog."));

    for (const auto& binding : m_hotkeyMgr->bindings()) {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(binding.label));
        auto* keyBtn = new QPushButton(binding.currentKeys);
        keyBtn->setMinimumWidth(120);

        connect(keyBtn, &QPushButton::clicked, this, [this, binding, keyBtn]() {
            // Simple key capture: show message box, in production use a proper capture dialog
            keyBtn->setText("Press keys...");
            keyBtn->setEnabled(false);
            // TODO: In production, install event filter for key capture
            QMessageBox::information(this, "Rebind",
                QString("Key capture dialog placeholder.\nCurrent: %1\n\n"
                        "In production: press the new key combination to rebind.")
                .arg(binding.currentKeys));
            keyBtn->setText(binding.currentKeys);
            keyBtn->setEnabled(true);
        });

        row->addWidget(keyBtn);
        row->addStretch();
        layout->addLayout(row);
    }

    auto* resetBtn = new QPushButton("Reset All to Defaults");
    layout->addWidget(resetBtn);
    layout->addStretch();
    return page;
}

QWidget* SettingsPanel::createAreasTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel("Translation Areas:"));
    // Area list placeholder
    auto* areaList = new QLabel("No areas configured. Use Ctrl+Shift+A "
                                 "or the tray menu to select areas.");
    areaList->setWordWrap(true);
    layout->addWidget(areaList);
    layout->addStretch();
    return page;
}

void SettingsPanel::loadStyle() {
    m_pendingStyle = m_config->loadStyle();

    m_textColorBtn->setStyleSheet(
        QString("background-color: %1; border: 1px solid #666;").arg(m_pendingStyle.textColor.name()));
    m_fontSizeSpin->setValue(m_pendingStyle.font.pointSize());
    int idx = m_fontCombo->findText(m_pendingStyle.font.family());
    if (idx >= 0) m_fontCombo->setCurrentIndex(idx);
    m_bgColorBtn->setStyleSheet(
        QString("background-color: %1; border: 1px solid #666;").arg(m_pendingStyle.backgroundColor.name()));
    m_alphaSlider->setValue(m_pendingStyle.backgroundAlpha);
    m_borderRadiusSpin->setValue(m_pendingStyle.borderRadius);
    m_borderColorBtn->setStyleSheet(
        QString("background-color: %1; border: 1px solid #666;").arg(m_pendingStyle.borderColor.name()));
    m_borderWidthSpin->setValue(m_pendingStyle.borderWidth);

    updatePreview();
}

void SettingsPanel::updatePreview() {
    QColor bg = m_pendingStyle.backgroundColor;
    bg.setAlpha(static_cast<int>(m_pendingStyle.backgroundAlpha * 255 / 100));
    m_previewLabel->setStyleSheet(QString(
        "background-color: %1; color: %2; border: %3px solid %4; border-radius: %5px; "
        "font-family: \"%6\"; font-size: %7pt; padding: 8px;")
        .arg(bg.name(QColor::HexArgb))
        .arg(m_pendingStyle.textColor.name())
        .arg(m_pendingStyle.borderWidth)
        .arg(m_pendingStyle.borderColor.name())
        .arg(m_pendingStyle.borderRadius)
        .arg(m_pendingStyle.font.family())
        .arg(m_pendingStyle.font.pointSize()));
}

void SettingsPanel::applyStyle() {
    m_config->saveStyle(m_pendingStyle);
    m_config->setSourceLang("auto"); // TODO: from UI
    m_config->setTargetLang("zh");   // TODO: from UI

    // Update active translator
    QString activeName = m_serviceCombo->currentData().toString();
    m_config->setActiveTranslator(activeName);

    // Save translator configs
    for (auto* t : m_translators) {
        for (const auto& field : t->configFields()) {
            m_config->setTranslatorConfig(t->name(), field.key, t->getConfig(field.key));
        }
    }

    emit styleChanged(m_pendingStyle);
    m_bus->styleChanged(m_pendingStyle);
}
```

- [ ] **Step 4: Commit**

```bash
git add src/ui/settings/
git commit -m "feat: add settings panel with live preview"
```

---

### Task 16: Application Class — Lifecycle Integration

**Files:**
- Create: `src/app/Application.h`
- Create: `src/app/Application.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Write Application.h**

```cpp
#pragma once

#include <QtWidgets/QApplication>
#include <QtCore/QTimer>
#include <memory>
#include "common/Types.h"

class SignalBus;
class DxgiCaptureEngine;
class WindowsOcrEngine;
class TranslatorManager;
class LayoutEngine;
class OverlayManager;
class TrayManager;
class HotkeyManager;
class SettingsPanel;
class Config;

class Application : public QObject {
    Q_OBJECT

public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    bool initialize(int argc, char* argv[]);

private slots:
    void onHotkeyTriggered(const QString& id);
    void onModeChanged(Mode mode);
    void onSnapshotRequested();
    void onAreaConfirmed(const SelectionArea& area);
    void onGlobalVisibilityToggle();
    void onSettingsRequested();
    void onFrameReady(const QImage& frame, const QRect& region);
    void onOcrCompleted(const OCRResult& result);
    void onTranslationReady(const QString& original, const QString& translated,
                            const QRect& sourceRect);
    void onStyleChanged(const StyleConfig& style);

private:
    void setMode(Mode mode);
    void processFrame();
    void processOcrResult(const OCRResult& result);

    // Core
    SignalBus*          m_bus     = nullptr;
    DxgiCaptureEngine*  m_capture = nullptr;
    WindowsOcrEngine*   m_ocr     = nullptr;
    TranslatorManager*  m_translator = nullptr;
    LayoutEngine*       m_layout  = nullptr;

    // UI
    OverlayManager*     m_overlays = nullptr;
    TrayManager*        m_tray     = nullptr;
    HotkeyManager*      m_hotkey   = nullptr;
    SettingsPanel*      m_settings = nullptr;
    Config*             m_config   = nullptr;

    // State
    Mode                    m_mode = Mode::Snapshot;
    QVector<SelectionArea>  m_areas;
    QTimer*                 m_captureTimer = nullptr;
    bool                    m_globalVisible = true;

    // Real-time tracking
    QHash<int, int>         m_textToOverlay;  // text hash → overlay id
};
```

- [ ] **Step 2: Write Application.cpp** (core integration logic)

```cpp
#include "Application.h"
#include "app/SignalBus.h"
#include "core/capture/DxgiCaptureEngine.h"
#include "core/ocr/WindowsOcrEngine.h"
#include "core/translate/TranslatorManager.h"
#include "core/translate/plugins/DeepLTranslator.h"
#include "core/translate/plugins/OpenAITranslator.h"
#include "engine/layout/LayoutEngine.h"
#include "engine/hotkey/HotkeyManager.h"
#include "ui/overlay/OverlayManager.h"
#include "ui/tray/TrayManager.h"
#include "ui/settings/SettingsPanel.h"
#include "ui/selector/AreaSelector.h"
#include "common/Config.h"
#include <QtCore/QDebug>
#include <QtCore/QHash>
#include <QtGui/QScreen>
#include <QtWidgets/QMessageBox>

Application::Application(QObject* parent) : QObject(parent) {}

Application::~Application() {
    // Save mode before exit
    if (m_config) m_config->setLastMode(m_mode);
}

bool Application::initialize(int argc, char* argv[]) {
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    // 1. Config
    m_config = new Config("ScreenLingo", "ScreenLingo", this);

    // 2. SignalBus
    m_bus = new SignalBus(this);

    // 3. Core modules
    m_capture = new DxgiCaptureEngine(this);
    if (!m_capture->initialize()) {
        QMessageBox::critical(nullptr, "ScreenLingo",
            "Failed to initialize screen capture. DXGI may not be available.");
        return false;
    }

    m_ocr = new WindowsOcrEngine(this);
    if (!m_ocr->initialize("auto")) {
        QMessageBox::critical(nullptr, "ScreenLingo",
            "Failed to initialize OCR engine. Windows 10 or language pack required.");
        return false;
    }

    m_translator = new TranslatorManager(this);
    m_translator->registerPlugin(std::make_unique<DeepLTranslator>());
    m_translator->registerPlugin(std::make_unique<OpenAITranslator>());

    // Restore translator config
    for (auto* plugin : m_translator->plugins()) {
        for (const auto& field : plugin->configFields()) {
            QString val = m_config->translatorConfig(plugin->name(), field.key);
            if (!val.isEmpty()) plugin->setConfig(field.key, val);
        }
    }
    QString lastTranslator = m_config->activeTranslator();
    if (!lastTranslator.isEmpty()) m_translator->setActive(lastTranslator);

    m_layout = new LayoutEngine;

    // 4. UI modules
    m_overlays = new OverlayManager(this);
    m_tray     = new TrayManager(this);
    m_tray->initialize();

    m_hotkey   = new HotkeyManager(this);
    QApplication::instance()->installNativeEventFilter(m_hotkey);

    // Register default hotkeys
    QVector<HotkeyBinding> defaultBindings = {
        {"mode_toggle",   "Toggle Mode",         "Ctrl+Shift+T"},
        {"area_select",   "Select Area",         "Ctrl+Shift+A"},
        {"global_hide",   "Show/Hide All",       "Ctrl+Shift+H"},
        {"long_press",    "Long-Press Translate", "Ctrl+Shift+G"},
        {"settings",      "Open Settings",       "Ctrl+Shift+S"},
        {"snapshot_once", "Single Snapshot",     "Ctrl+Shift+Q"},
    };
    m_hotkey->setBindings(m_config->loadHotkeys(defaultBindings));
    for (const auto& b : m_hotkey->bindings()) {
        if (!m_hotkey->registerBinding(b)) {
            qWarning() << "Failed to register hotkey:" << b.id << b.currentKeys;
        }
    }

    // 5. Wire signals (SignalBus connections)
    connect(m_capture, &DxgiCaptureEngine::captureError, this, [](const QString& msg) {
        qWarning() << "Capture error:" << msg;
    });

    connect(m_ocr, &WindowsOcrEngine::recognitionComplete,
            this, &Application::onOcrCompleted);

    connect(m_ocr, &WindowsOcrEngine::recognitionError, this, [](const QString& msg) {
        qWarning() << "OCR error:" << msg;
    });

    connect(m_translator, &TranslatorManager::translationReady,
            this, &Application::onTranslationReady);

    connect(m_translator, &TranslatorManager::translationError,
            this, [](const QString& msg) {
        qWarning() << "Translation error:" << msg;
    });

    connect(m_hotkey, &HotkeyManager::hotkeyTriggered,
            this, &Application::onHotkeyTriggered);

    connect(m_tray, &TrayManager::modeChangeRequested,
            this, &Application::onModeChanged);
    connect(m_tray, &TrayManager::areaSelectRequested,
            this, [this]() { onHotkeyTriggered("area_select"); });
    connect(m_tray, &TrayManager::globalVisibilityToggleRequested,
            this, &Application::onGlobalVisibilityToggle);
    connect(m_tray, &TrayManager::settingsRequested,
            this, &Application::onSettingsRequested);
    connect(m_tray, &TrayManager::exitRequested,
            qApp, &QApplication::quit);

    // 6. Capture timer for real-time mode
    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(100);

    // 7. Restore last mode
    Mode savedMode = m_config->lastMode();
    setMode(savedMode);

    // 8. Restore areas
    m_areas = m_config->loadAreas();

    return true;
}

void Application::setMode(Mode mode) {
    m_mode = mode;
    m_tray->updateIcon(mode);
    m_config->setLastMode(mode);
    m_bus->modeChanged(mode);

    switch (mode) {
    case Mode::RealTime:
        m_captureTimer->start();
        break;
    case Mode::Snapshot:
        m_captureTimer->stop();
        break;
    case Mode::Pause:
        m_captureTimer->stop();
        break;
    }
}

void Application::onHotkeyTriggered(const QString& id) {
    if (id == "mode_toggle") {
        // Cycle: RealTime → Snapshot → Pause → RealTime
        switch (m_mode) {
        case Mode::RealTime:  setMode(Mode::Snapshot); break;
        case Mode::Snapshot:  setMode(Mode::Pause);    break;
        case Mode::Pause:     setMode(Mode::RealTime);  break;
        }
    } else if (id == "area_select") {
        auto* selector = new AreaSelector(0);  // primary monitor
        connect(selector, &AreaSelector::areaConfirmed,
                this, &Application::onAreaConfirmed);
        connect(selector, &AreaSelector::cancelled, selector, &QWidget::deleteLater);
    } else if (id == "global_hide") {
        onGlobalVisibilityToggle();
    } else if (id == "settings") {
        onSettingsRequested();
    } else if (id == "snapshot_once") {
        onSnapshotRequested();
    }
}

void Application::onModeChanged(Mode mode) {
    setMode(mode);
}

void Application::onSnapshotRequested() {
    if (m_areas.isEmpty()) return;
    const auto& area = m_areas.first();
    QImage frame = m_capture->captureRegion(area.geometry, area.screenIndex);
    if (!frame.isNull()) {
        m_bus->frameReady(frame, area.geometry);
        m_ocr->recognize(frame);
    }
}

void Application::onAreaConfirmed(const SelectionArea& area) {
    m_areas.clear();  // v1.0: single area only
    m_areas.append(area);
    m_config->saveAreas(m_areas);
}

void Application::onGlobalVisibilityToggle() {
    m_globalVisible = !m_globalVisible;
    m_bus->globalVisibilityChanged(m_globalVisible);
    if (m_globalVisible) m_overlays->showAll();
    else                 m_overlays->removeAll();
}

void Application::onSettingsRequested() {
    if (!m_settings) {
        m_settings = new SettingsPanel(
            m_translator->plugins(),
            m_hotkey, m_config, m_bus);
        connect(m_settings, &SettingsPanel::styleChanged,
                this, &Application::onStyleChanged);
    }
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void Application::onOcrCompleted(const OCRResult& result) {
    if (result.fullText.isEmpty()) return;

    QString srcLang = m_config->sourceLang();
    QString tgtLang = m_config->targetLang();
    m_translator->translate(result.fullText, srcLang, tgtLang);
}

void Application::onTranslationReady(const QString& original,
                                      const QString& translated,
                                      const QRect& sourceRect) {
    if (!m_globalVisible) return;

    // Compute layout
    LayoutRequest req;
    req.sourceRect       = sourceRect;
    req.translatedText   = translated;
    req.preferredFontSize = m_config->loadStyle().font.pointSize();

    QScreen* screen = QApplication::primaryScreen();
    QRect screenBounds = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);

    LayoutResult layout = m_layout->compute(req,
        m_overlays->existingBubbleRects(), screenBounds);

    // Show overlay
    int textHash = qHash(original);
    if (m_textToOverlay.contains(textHash)) {
        m_overlays->updateTranslation(m_textToOverlay[textHash], translated, layout);
    } else {
        int id = m_overlays->showTranslation(layout, translated);
        m_textToOverlay[textHash] = id;
    }
}

void Application::onStyleChanged(const StyleConfig& style) {
    m_overlays->updateAllStyles(style);
}

void Application::onFrameReady(const QImage& /*frame*/, const QRect& /*region*/) {
    // Processing handled inline in snapshot request
}
```

- [ ] **Step 3: Update main.cpp**

```cpp
#include <QtWidgets/QApplication>
#include "app/Application.h"

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationName("ScreenLingo");
    app.setOrganizationName("ScreenLingo");
    app.setQuitOnLastWindowClosed(false);

    Application screenLingo;
    if (!screenLingo.initialize(argc, argv)) {
        return 1;
    }

    return app.exec();
}
```

- [ ] **Step 4: Commit**

```bash
git add src/app/ src/main.cpp
git commit -m "feat: add Application class wiring all modules together"
```

---

### Task 17: Build and Verify

- [ ] **Step 1: Configure the project**

```bash
cmake -B build -S . -DCMAKE_PREFIX_PATH=<path-to-qt6-install>
```

Expected: CMake configuration succeeds, all Qt6 components found, WinRT/DXGI libraries found.

- [ ] **Step 2: Build**

```bash
cmake --build build
```

Expected: Compilation succeeds with zero errors. Warnings acceptable.

- [ ] **Step 3: Run and verify basic startup**

```bash
./build/Debug/ScreenLingo.exe
```

Check:
- Application starts without crash
- System tray icon appears (green dot)
- Right-click tray icon shows full menu
- Hotkeys registered (Win+R → verify no conflicts reported in console)

- [ ] **Step 4: Verify mode switching**

In the running application:
- Press Ctrl+Shift+T → icon should stay green (now Snapshot mode)
- Press Ctrl+Shift+T again → icon turns yellow (Pause mode)
- Press Ctrl+Shift+T again → back to green (Real-time mode)
- Test from tray menu as well

- [ ] **Step 5: Commit**

```bash
git add .
git commit -m "build: verify full project builds and runs"
```

---

## Dependency Graph

```
Task 1 (scaffold)
 └─► Task 2 (types)
      ├─► Task 4 (config)
      ├─► Task 5 (capture)
      │    └─► Task 16 (app integration) ◄── all tasks
      ├─► Task 6 (OCR)
      │    └─► Task 16
      ├─► Task 7 (translator interface)
      │    ├─► Task 8 (DeepL)
      │    ├─► Task 9 (OpenAI)
      │    └─► Task 15 (settings)
      ├─► Task 10 (layout engine)
      │    └─► Task 12 (overlays)
      ├─► Task 11 (hotkeys)
      │    ├─► Task 15 (settings)
      │    └─► Task 16
      ├─► Task 3 (SignalBus)
      │    └─► Tasks 15, 16
      ├─► Task 12 (overlays)
      │    └─► Task 16
      ├─► Task 13 (area selector)
      │    └─► Task 16
      ├─► Task 14 (tray)
      │    └─► Task 16
      └─► Task 15 (settings)
           └─► Task 16
```

**Parallel execution opportunities:**
- Tasks 3, 4, 5, 6, 7, 10, 11, 13, 14 can run in parallel (no deps on each other)
- Tasks 8, 9 depend on 7
- Task 12 depends on 10
- Task 15 depends on 4, 7, 11
- Task 16 depends on ALL

---

## Test Strategy

Each task's correctness is verified by:
1. **Compilation** — `cmake --build build` succeeds
2. **Interface correctness** — types match across modules (enforced by compiler)
3. **Module isolation** — no circular includes, each module testable independently
4. **Integration** — Task 16 verifies all modules wire together; Task 17 verifies end-to-end

For v1.0, system tests are manual (Task 17 checkpoints). Automated tests deferred to v1.1.
