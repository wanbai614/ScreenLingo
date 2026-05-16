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
#include "ui/selector/AreaSelector.h"
#include "common/Config.h"

// SettingsPanel is being created in a parallel task.
// When it lands the include will be picked up automatically.
#if __has_include("ui/settings/SettingsPanel.h")
#include "ui/settings/SettingsPanel.h"
#define HAS_SETTINGS_PANEL 1
#else
#define HAS_SETTINGS_PANEL 0
#endif

#include <QtCore/QDebug>
#include <QtWidgets/QMessageBox>
#include <QtGui/QScreen>
#include <QtGui/QCursor>

Application::Application(QObject* parent) : QObject(parent) {}

Application::~Application() {
    if (m_config) m_config->setLastMode(m_mode);
}

bool Application::initialize() {
    // 1. Config
    m_config = new Config("ScreenLingo", "ScreenLingo");

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

    // Restore translator configs
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

    // Register hotkeys from config
    QVector<HotkeyBinding> defaultBindings = {
        {"mode_toggle",   "Toggle Mode",           "Ctrl+Shift+T"},
        {"area_select",   "Select Area",           "Ctrl+Shift+A"},
        {"global_hide",   "Show/Hide All",         "Ctrl+Shift+H"},
        {"long_press",    "Long-Press Translate",  "Ctrl+Shift+G"},
        {"settings",      "Open Settings",         "Ctrl+Shift+S"},
        {"snapshot_once", "Single Snapshot",       "Ctrl+Shift+Q"},
    };
    m_hotkey->setBindings(m_config->loadHotkeys(defaultBindings));
    for (const auto& b : m_hotkey->bindings()) {
        if (!m_hotkey->registerBinding(b)) {
            qWarning() << "Failed to register hotkey:" << b.id << b.currentKeys;
        }
    }

    // 5. Wire signals

    // OCR
    connect(m_ocr, &WindowsOcrEngine::recognitionComplete,
            this, &Application::onOcrCompleted);
    connect(m_ocr, &WindowsOcrEngine::recognitionError, this, [](const QString& msg) {
        qWarning() << "OCR error:" << msg;
    });

    // Translation (TranslatorManager signal has only 2 params, no QRect)
    connect(m_translator, &TranslatorManager::translationReady,
            this, &Application::onTranslationReady);
    connect(m_translator, &TranslatorManager::translationError,
            this, [](const QString& msg) {
        qWarning() << "Translation error:" << msg;
    });

    // Hotkeys
    connect(m_hotkey, &HotkeyManager::hotkeyTriggered,
            this, &Application::onHotkeyTriggered);

    // Tray
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
    // Real-time mode frame processing loop will be wired in v1.1

    // 7. Restore last mode and areas
    setMode(m_config->lastMode());
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
        switch (m_mode) {
        case Mode::RealTime:  setMode(Mode::Snapshot); break;
        case Mode::Snapshot:  setMode(Mode::Pause);    break;
        case Mode::Pause:     setMode(Mode::RealTime); break;
        }
    } else if (id == "area_select") {
        int screenIdx = 0;
        QScreen* screen = QApplication::screenAt(QCursor::pos());
        if (screen) screenIdx = QApplication::screens().indexOf(screen);
        auto* selector = new AreaSelector(screenIdx);
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
    if (!area.enabled) return;

    QImage frame = m_capture->captureRegion(area.geometry, area.screenIndex);
    if (!frame.isNull()) {
        // Store source rect so onTranslationReady knows where to place the bubble.
        m_lastSourceRect = area.geometry;
        m_ocr->recognize(frame);
    }
}

void Application::onAreaConfirmed(const QRect& area, int screenIndex) {
    m_areas.clear();

    static int nextAreaId = 1;
    SelectionArea sel;
    sel.id          = nextAreaId++;
    sel.screenIndex = screenIndex;
    sel.geometry    = area;
    sel.enabled     = true;
    m_areas.append(sel);

    m_config->saveAreas(m_areas);
}

void Application::onGlobalVisibilityToggle() {
    m_globalVisible = !m_globalVisible;
    m_bus->globalVisibilityChanged(m_globalVisible);
    if (m_globalVisible) m_overlays->showAll();
    else                 m_overlays->removeAll();
}

void Application::onSettingsRequested() {
#if HAS_SETTINGS_PANEL
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
#else
    qInfo() << "Settings panel not yet available (created in parallel task).";
#endif
}

void Application::onOcrCompleted(const OCRResult& result) {
    if (result.fullText.isEmpty()) return;
    QString srcLang = m_config->sourceLang();
    QString tgtLang = m_config->targetLang();
    m_translator->translate(result.fullText, srcLang, tgtLang);
}

void Application::onTranslationReady(const QString& original,
                                      const QString& translated) {
    if (!m_globalVisible) return;

    QRect sourceRect = m_lastSourceRect;
    if (sourceRect.isNull()) return;

    LayoutRequest req;
    req.sourceRect        = sourceRect;
    req.translatedText    = translated;
    req.preferredFontSize = m_config->loadStyle().font.pointSize();

    QScreen* screen = QApplication::primaryScreen();
    QRect screenBounds = screen ? screen->geometry()
                                : QRect(0, 0, 1920, 1080);

    LayoutResult layout = m_layout->compute(req,
        m_overlays->existingBubbleRects(), screenBounds);

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
