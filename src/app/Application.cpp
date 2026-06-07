#include "Application.h"
#include "app/SignalBus.h"
#include "core/capture/DxgiCaptureEngine.h"
#include "core/ocr/IOCREngine.h"
#include "core/ocr/WindowsOcrEngine.h"
#if HAS_PADDLE_OCR
#include "core/ocr/PaddleOCREngine.h"
#endif
#include "core/ocr/PaddleOCRSubprocessEngine.h"
#include "core/translate/TranslatorManager.h"
#include "core/translate/plugins/DeepLTranslator.h"
#include "core/translate/plugins/OpenAITranslator.h"
#include "core/translate/plugins/DeepSeekTranslator.h"
#include "core/translate/plugins/OllamaTranslator.h"
#include "core/translate/plugins/LocalDictTranslator.h"
#include "core/translate/plugins/BaiduTranslator.h"
#include "core/translate/plugins/GoogleTranslator.h"
#include "engine/layout/LayoutEngine.h"
#include "engine/hotkey/HotkeyManager.h"
#include "ui/overlay/OverlayManager.h"
#include "ui/tray/TrayManager.h"
#include "ui/floating/FloatingToolbar.h"
#include "ui/selector/AreaSelector.h"
#include "ui/overlay/AreaOverlay.h"
#include "core/selection/MouseSelectionMonitor.h"
#include "ui/popup/TranslationPopup.h"
#include "common/Config.h"
#include "common/LanguageManager.h"

// SettingsPanel is being created in a parallel task.
// When it lands the include will be picked up automatically.
#if __has_include("ui/settings/SettingsPanel.h")
#include "ui/settings/SettingsPanel.h"
#define HAS_SETTINGS_PANEL 1
#else
#define HAS_SETTINGS_PANEL 0
#endif

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QStandardPaths>
#include <QtWidgets/QMessageBox>
#include <QtGui/QScreen>
#include <QtGui/QCursor>
#include <numeric>

// --- Diagnostic log helper ---
static void appLog(const QString& msg) {
    static QFile f;
    if (!f.isOpen()) {
        f.setFileName(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/screenlingo_debug.log");
        f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    QTextStream ts(&f);
    ts << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << "  " << msg << "\n";
    ts.flush();
}

// --- Text grouping: PaddleOCR-inspired text block detection ---
// Phase 1: merge boxes into "words" (close horizontal gaps on same line)
// Phase 2: merge words into "lines" (common Y baseline)
// Phase 3: merge lines into "blocks" (close vertical spacing → paragraphs)
struct TextGroup {
    QString text;
    QRect   rect;  // in screen coordinates
};

static QVector<TextGroup> groupTextBoxes(const QVector<TextBox>& boxes,
                                          const QRect& captureRect) {
    if (boxes.isEmpty()) return {};

    // --- Compute text metrics (median height → robust to outliers) ---
    QVector<int> heights;
    for (const auto& b : boxes)
        heights.append(b.boundingRect.height());
    std::sort(heights.begin(), heights.end());
    int medianH = heights[heights.size() / 2];
    medianH = qBound(8, medianH, 80);

    // Detection thresholds (PaddleOCR-style adaptive ratios)
    const int kSameLine   = qMax(medianH * 2 / 5, 4);   // Y tolerance for same line
    const int kWordGap    = qMax(medianH * 3 / 5, 5);   // max gap to merge into word
    const int kBlockGap   = qMax(medianH * 3 / 2, 12);  // max gap to merge lines→block

    auto screenRect = [&](const TextBox& b) {
        return QRect(captureRect.x() + b.boundingRect.x(),
                     captureRect.y() + b.boundingRect.y(),
                     b.boundingRect.width(), b.boundingRect.height());
    };

    // --- Phase 1: sort by Y then X ---
    QVector<int> indices(boxes.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        int ya = boxes[a].boundingRect.center().y();
        int yb = boxes[b].boundingRect.center().y();
        if (std::abs(ya - yb) <= kSameLine)
            return boxes[a].boundingRect.left() < boxes[b].boundingRect.left();
        return ya < yb;
    });

    // --- Phase 2: merge boxes on the same line, splitting at large gaps ---
    QVector<TextGroup> words;
    TextGroup cur;
    for (int idx : indices) {
        const TextBox& b = boxes[idx];
        QString t = b.text.trimmed();
        if (t.isEmpty()) continue;

        QRect sr = screenRect(b);
        bool sameWord = !cur.text.isEmpty()
            && std::abs(cur.rect.center().y() - sr.center().y()) <= kSameLine
            && (sr.left() - cur.rect.right()) <= kWordGap;

        if (sameWord) {
            cur.text += " " + t;
            cur.rect = cur.rect.united(sr);
        } else {
            if (!cur.text.isEmpty()) words.append(cur);
            cur.text = t;
            cur.rect = sr;
        }
    }
    if (!cur.text.isEmpty()) words.append(cur);

    // --- Phase 3: merge consecutive words on the same line ---
    QVector<TextGroup> lines;
    cur = TextGroup{};
    for (const auto& w : words) {
        bool sameLine = !cur.text.isEmpty()
            && std::abs(cur.rect.center().y() - w.rect.center().y()) <= kSameLine;

        if (sameLine) {
            cur.text += " " + w.text;
            cur.rect = cur.rect.united(w.rect);
        } else {
            if (!cur.text.isEmpty()) lines.append(cur);
            cur = w;
        }
    }
    if (!cur.text.isEmpty()) lines.append(cur);

    return lines;
}

// Lightweight UI-mode grouping: merge only boxes on the SAME line that are
// very close together (like parts of one UI label). Keeps independent
// UI elements (buttons, menu items) as separate translation units.
static QVector<TextGroup> groupUIBoxes(const QVector<TextBox>& boxes,
                                         const QRect& captureRect) {
    if (boxes.isEmpty()) return {};

    QVector<int> heights;
    for (const auto& b : boxes)
        heights.append(b.boundingRect.height());
    std::sort(heights.begin(), heights.end());
    int medianH = qBound(8, heights[heights.size() / 2], 80);

    // Tighter thresholds for UI mode — only merge very close neighbors
    const int kSameLine = qMax(medianH, 6);           // Y tolerance: 1 line height
    const int kUIGap    = qMax(medianH / 2, 3);        // small gap → same UI element

    auto screenRect = [&](const TextBox& b) {
        return QRect(captureRect.x() + b.boundingRect.x(),
                     captureRect.y() + b.boundingRect.y(),
                     b.boundingRect.width(), b.boundingRect.height());
    };

    QVector<int> indices(boxes.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        int ya = boxes[a].boundingRect.center().y();
        int yb = boxes[b].boundingRect.center().y();
        if (std::abs(ya - yb) <= kSameLine)
            return boxes[a].boundingRect.left() < boxes[b].boundingRect.left();
        return ya < yb;
    });

    QVector<TextGroup> groups;
    TextGroup cur;
    for (int idx : indices) {
        const TextBox& b = boxes[idx];
        QString t = b.text.trimmed();
        if (t.isEmpty()) continue;

        QRect sr = screenRect(b);
        bool sameGroup = !cur.text.isEmpty()
            && std::abs(cur.rect.center().y() - sr.center().y()) <= kSameLine
            && (sr.left() - cur.rect.right()) <= kUIGap;

        if (sameGroup) {
            cur.text += " " + t;
            cur.rect = cur.rect.united(sr);
        } else {
            if (!cur.text.isEmpty()) groups.append(cur);
            cur.text = t;
            cur.rect = sr;
        }
    }
    if (!cur.text.isEmpty()) groups.append(cur);

    return groups;
}

// Filter OCR garbage: text that is mostly random symbols/punctuation
// from a mismatched charset (e.g., EN model reading Chinese text)
static bool isLikelyGarbage(const QString& text) {
    if (text.isEmpty()) return true;
    int alpha = 0, digit = 0, space = 0, other = 0, upper = 0, lower = 0;
    QSet<QChar> uniqueChars;
    for (const QChar& c : text) {
        uniqueChars.insert(c);
        if (c.isLetter()) {
            ++alpha;
            if (c.isUpper()) ++upper; else ++lower;
        } else if (c.isDigit()) ++digit;
        else if (c.isSpace()) ++space;
        else ++other;
    }
    int total = alpha + digit + space + other;
    if (total < 2) return true;
    // >60% symbols → likely noise (relaxed from 40%)
    if (other > total * 3 / 5) return true;
    // No letters AND has symbols AND short → likely noise
    if (alpha == 0 && other > 0 && total <= 4) return true;
    // For 10+ letter words: if >90% chars are distinct → likely random
    if (alpha >= 10 && uniqueChars.size() * 10 > total * 9) return true;
    return false;
}

// Check if a group of text is mostly garbage words (EN model reading Chinese)
static bool isMostlyGarbage(const QString& groupText) {
    QStringList words = groupText.split(' ', Qt::SkipEmptyParts);
    if (words.size() < 3) return false;  // too few words to judge
    int garbageWords = 0;
    for (const QString& w : words) {
        if (isLikelyGarbage(w)) ++garbageWords;
    }
    // If >60% of words look like garbage, skip the whole group
    return garbageWords * 5 > words.size() * 3;
}

Application::Application(QObject* parent) : QObject(parent) {}

Application::~Application() {
    if (m_config) m_config->setLastMode(m_mode);
}

bool Application::initialize() {
    appLog("=== ScreenLingo init start ===");
    appLog("Log file: " + QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + "/screenlingo_debug.log");

    // 1. Config
    m_config = new Config("ScreenLingo", "ScreenLingo");

    // 2. SignalBus
    m_bus = new SignalBus(this);

    // 2.5 Language - init before UI
    LanguageManager::instance()->initialize(
        m_config->translatorConfig("app", "language"));

    // 3. Core modules
    m_capture = new DxgiCaptureEngine(this);
    if (!m_capture->initialize()) {
        QMessageBox::critical(nullptr, "ScreenLingo",
            tr("Failed to initialize screen capture. DXGI may not be available."));
        return false;
    }

    // Select OCR engine based on user config
    QString ocrEngineName = m_config->ocrEngine();
    m_currentOcrEngine = ocrEngineName;
    if (ocrEngineName == QStringLiteral("paddle")) {
        m_ocr = new PaddleOCRSubprocessEngine(this);
        if (!m_ocr->initialize("auto")) {
            qWarning() << "PaddleOCR subprocess init failed, falling back to Windows OCR";
            delete m_ocr;
            m_ocr = new WindowsOcrEngine(this);
            m_ocr->initialize("auto");
            m_currentOcrEngine = "windows";
        }
    } else {
        m_ocr = new WindowsOcrEngine(this);
        if (!m_ocr->initialize("auto")) {
            QMessageBox::critical(nullptr, "ScreenLingo",
                tr("Failed to initialize OCR engine. Windows 10 or language pack required."));
            return false;
        }
    }

    m_translator = new TranslatorManager(this);
    m_translator->registerPlugin(std::make_unique<DeepLTranslator>());
    m_translator->registerPlugin(std::make_unique<OpenAITranslator>());
    m_translator->registerPlugin(std::make_unique<DeepSeekTranslator>());
    m_translator->registerPlugin(std::make_unique<OllamaTranslator>());
    m_translator->registerPlugin(std::make_unique<LocalDictTranslator>());
    m_translator->registerPlugin(std::make_unique<BaiduTranslator>());
    m_translator->registerPlugin(std::make_unique<GoogleTranslator>());

    // Restore translator configs
    for (auto* plugin : m_translator->plugins()) {
        for (const auto& field : plugin->configFields()) {
            QString val = m_config->translatorConfig(plugin->name(), field.key);
            if (!val.isEmpty()) plugin->setConfig(field.key, val);
        }
    }
    QString lastTranslator = m_config->activeTranslator();
    if (!lastTranslator.isEmpty()) m_translator->setActive(lastTranslator);

    // Apply active prompt preset (overrides saved systemPrompt for active translator)
    {
        QString promptId = m_config->activePromptId();
        auto presets = m_config->loadPromptPresets();
        for (const auto& p : presets) {
            if (p.id == promptId) {
                auto* active = m_translator->active();
                if (active) {
                    QString prompt = p.prompt;
                    // Inject hard format rule
                    static const QString kHardRule =
                        "\nSTRICT: Output translation only. "
                        "No bullet points, no glossaries, no notes, no explanations. "
                        "If you cannot translate, output \"??\".";
                    if (!prompt.contains("STRICT:"))
                        prompt += kHardRule;
                    active->setConfig("systemPrompt", prompt);
                    m_config->setTranslatorConfig(active->name(), "systemPrompt", prompt);
                }
                break;
            }
        }
    }

    m_layout = new LayoutEngine;

    // 4. UI modules
    m_overlays = new OverlayManager(this);
    m_tray     = new TrayManager(this);
    m_tray->initialize();

    m_floating = new FloatingToolbar();
    m_floating->show();

    // Selection-translate popup
    m_selPopup = new TranslationPopup();
    m_selMonitor = new MouseSelectionMonitor(this);
    connect(m_selMonitor, &MouseSelectionMonitor::textSelected,
            this, &Application::onTextSelected);


    // Persistent area border overlay (non-modal, click-through when idle)
    m_areaOverlay = new AreaOverlay();
    connect(m_areaOverlay, &AreaOverlay::areaChanged,
            this, &Application::onAreaOverlayChanged);
    connect(m_areaOverlay, &AreaOverlay::editCancelled,
            this, &Application::onAreaEditCancelled);
    m_hotkey   = new HotkeyManager(this);
    QApplication::instance()->installNativeEventFilter(m_hotkey);

    // Register hotkeys from config
    QVector<HotkeyBinding> defaultBindings = {
        {"mode_toggle",      tr("Toggle Mode"),           "Ctrl+Shift+T"},
        {"area_select",      tr("Select Area"),           "Ctrl+Shift+A"},
        {"global_hide",      tr("Show/Hide All"),         "Ctrl+Shift+H"},
        {"long_press",       tr("Long-Press Translate"),  "Ctrl+Shift+G"},
        {"settings",         tr("Open Settings"),         "Ctrl+Shift+S"},
        {"snapshot_once",    tr("Single Snapshot"),       "Ctrl+Shift+Q"},
        {"interact_toggle",  tr("Copy Text Mode"),        "Ctrl+Shift+C"},
    };
    m_hotkey->setBindings(m_config->loadHotkeys(defaultBindings));
    for (const auto& b : m_hotkey->bindings()) {
        if (!m_hotkey->registerBinding(b)) {
            qWarning() << "Failed to register hotkey:" << b.id << b.currentKeys;
        }
    }

    // 5. Wire signals

    // OCR (use IOCREngine base type for both WindowsOcrEngine and PaddleOCREngine)
    connect(m_ocr, &IOCREngine::recognitionComplete,
            this, &Application::onOcrCompleted);
    connect(m_ocr, &IOCREngine::recognitionError, this, [](const QString& msg) {
        appLog("OCR ERROR: " + msg);
        qWarning() << "OCR error:" << msg;
    });

    // Translation (TranslatorManager signal has only 2 params, no QRect)
    connect(m_translator, &TranslatorManager::translationReady,
            this, &Application::onTranslationReady);
    connect(m_translator, &TranslatorManager::translationError,
            this, [this](const QString& msg) {
        appLog("TRANSLATE ERROR: " + msg);
        qWarning() << "Translation error:" << msg;
        if (m_pendingTranslations > 0) --m_pendingTranslations;
        if (m_pendingTranslations == 0) flushRowLayout();
    });
    connect(m_translator, &TranslatorManager::visionResultReady,
            this, &Application::onVisionResultReady);

    // Hotkeys
    connect(m_hotkey, &HotkeyManager::hotkeyTriggered,
            this, &Application::onHotkeyTriggered);

    // Tray
    connect(m_tray, &TrayManager::modeChangeRequested,
            this, &Application::onModeChanged);
    connect(m_tray, &TrayManager::areaSelectRequested,
            this, &Application::launchAreaSelector);
    connect(m_tray, &TrayManager::globalVisibilityToggleRequested,
            this, &Application::onGlobalVisibilityToggle);
    connect(m_tray, &TrayManager::triggerActionRequested, this, [this]() {
        switch (m_mode) {
        case Mode::RealTime: setMode(Mode::Pause);    break;
        case Mode::Pause:    setMode(Mode::RealTime); break;
        case Mode::Snapshot: onSnapshotRequested();    break;
        }
    });

    // Floating toolbar
    connect(m_floating, &FloatingToolbar::triggerActionRequested, this, [this]() {
        switch (m_mode) {
        case Mode::RealTime: setMode(Mode::Pause);    break;
        case Mode::Pause:    setMode(Mode::RealTime); break;
        case Mode::Snapshot: onSnapshotRequested();    break;
        }
    });
    connect(m_floating, &FloatingToolbar::areaSelectRequested,
            this, &Application::launchAreaSelector);
    connect(m_floating, &FloatingToolbar::visibilityToggleRequested,
            this, &Application::onGlobalVisibilityToggle);
    connect(m_floating, &FloatingToolbar::settingsToggleRequested,
            this, [this]() {
        if (m_settings && m_settings->isVisible()) {
            m_settings->close();
        } else {
            onSettingsRequested();
        }
    });
    connect(m_tray, &TrayManager::settingsRequested,
            this, &Application::onSettingsRequested);
    connect(m_tray, &TrayManager::stopRequested,
            this, &Application::stopTranslation);
    connect(m_floating, &FloatingToolbar::stopRequested,
            this, &Application::stopTranslation);
    connect(m_floating, &FloatingToolbar::selTranslateRequested,
            this, &Application::toggleSelectionMode);
    connect(m_floating, &FloatingToolbar::dragModeToggleRequested, this, [this]() {
        m_dragMode = !m_dragMode;
        m_overlays->setInteractive(m_dragMode);
        m_floating->setDragMode(m_dragMode);
    });
    connect(m_tray, &TrayManager::languageChangeRequested,
            this, &Application::onLanguageChangeRequested);
    connect(m_tray, &TrayManager::selTranslateRequested,
            this, &Application::toggleSelectionMode);
    connect(m_tray, &TrayManager::exitRequested,
            qApp, &QApplication::quit);

    // 6. Capture timer for real-time mode
    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(200);
    connect(m_captureTimer, &QTimer::timeout,
            this, &Application::processRealtimeFrame);

    // Flush timeout: if translations don't all arrive within 15s, flush partial
    m_flushTimeout = new QTimer(this);
    m_flushTimeout->setSingleShot(true);
    m_flushTimeout->setInterval(15000);
    connect(m_flushTimeout, &QTimer::timeout, this, [this]() {
        appLog("Flush timeout: forcing partial flush");
        flushRowLayout();
    });

    // 7. Restore last mode and areas
    setMode(m_config->lastMode());
    m_areas = m_config->loadAreas();

    syncAreaOverlay();
    appLog(QString("Init done: mode=%1 areas=%2 activeTranslator=%3")
           .arg(static_cast<int>(m_mode))
           .arg(m_areas.size())
           .arg(m_translator->active() ? m_translator->active()->name() : "none"));

    return true;
}

void Application::setMode(Mode mode) {
    m_mode = mode;
    m_tray->updateIcon(mode);
    if (m_floating) m_floating->setMode(mode);
    m_config->setLastMode(mode);
    m_bus->modeChanged(mode);
    if (m_floating) m_floating->setPipelineStatus("idle");

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
        launchAreaSelector();
    } else if (id == "global_hide") {
        onGlobalVisibilityToggle();
    } else if (id == "settings") {
        onSettingsRequested();
    } else if (id == "snapshot_once") {
        onSnapshotRequested();
    } else if (id == "long_press") {
        toggleSelectionMode();
    } else if (id == "interact_toggle") {
        static bool s_interactive = false;
        s_interactive = !s_interactive;
        m_overlays->setInteractive(s_interactive);
        appLog(QString("Bubbles interactive: %1").arg(s_interactive));
    }
}

void Application::onModeChanged(Mode mode) {
    setMode(mode);
}

void Application::processRealtimeFrame() {
    if (m_selectionMode) return;  // selection mode active, skip area translation
    if (m_areas.isEmpty()) return;
    if (m_ocrBusy || m_pendingTranslations > 0) return;
    const auto& area = m_areas.first();
    if (!area.enabled) return;

    QImage frame = m_capture->captureRegion(area.geometry, area.screenIndex);
    if (frame.isNull()) return;

    // Pixel-level dedup: skip OCR if frame hasn't visually changed
    uint frameHash = qHashBits(frame.constBits(), frame.sizeInBytes());
    if (frameHash == m_lastFrameHash) return;
    m_lastFrameHash = frameHash;

    m_ocrBusy = true;
    m_lastSourceRect = area.geometry;
    m_ocr->recognize(frame);
}

void Application::onSnapshotRequested() {
    if (m_selectionMode) return;  // selection mode active, skip area translation
    appLog("Snapshot: enter");
    if (m_areas.isEmpty()) { appLog("Snapshot: no areas"); return; }
    const auto& area = m_areas.first();
    if (!area.enabled) { appLog("Snapshot: area disabled"); return; }

    // Ensure translations are visible
    if (!m_globalVisible) {
        m_globalVisible = true;
        if (m_floating) m_floating->setGlobalVisible(true);
        m_tray->setGlobalVisible(true);
        m_overlays->showAll();
    }

    // Bump generation counter — reject stale translation responses
    ++m_snapshotGen;

    // Clear existing overlays for snapshot mode
    m_overlays->removeAll();
    m_textToOverlay.clear();
    m_textSourceRects.clear();
    m_activeTextHashes.clear();
    m_translationCache.clear();
    m_translatedValues.clear();
    m_pendingTranslations = 0;
    m_pendingResults.clear();
    m_batchPending.clear();
    m_isBatched = false;
    m_translator->cancelAll();  // clear stale queue before new snapshot
    m_flushTimeout->stop();

    QImage frame = m_capture->captureRegion(area.geometry, area.screenIndex);
    if (frame.isNull()) { appLog("Snapshot: capture returned null"); return; }

    m_lastFrameHash = qHashBits(frame.constBits(), frame.sizeInBytes());
    m_lastSourceRect = area.geometry;

    // Read VLM setting each snapshot (user may have toggled in settings)
    if (m_config->vlmSnapshot()) {
        appLog(QString("Snapshot VLM: frame=%1x%2").arg(frame.width()).arg(frame.height()));
        if (m_floating) m_floating->setPipelineStatus("translating");
        m_translator->translateWithImage(frame,
            m_config->sourceLang(), m_config->targetLang());
    } else {
        if (m_floating) m_floating->setPipelineStatus("recognizing");
        m_ocr->recognize(frame);
        appLog(QString("Snapshot: OCR triggered, frame=%1x%2 mode=%3 vis=%4")
               .arg(frame.width()).arg(frame.height())
               .arg(static_cast<int>(m_mode)).arg(m_globalVisible));
    }
}

void Application::launchAreaSelector() {
    int screenIdx = 0;
    QScreen* screen = QApplication::screenAt(QCursor::pos());
    if (screen) screenIdx = QApplication::screens().indexOf(screen);
    auto* selector = new AreaSelector(screenIdx);

    // Re-edit mode: pre-fill with existing area so user can adjust in-place
    if (!m_areas.isEmpty()) {
        selector->setInitialRect(m_areas.first().geometry);
    }

    connect(selector, &AreaSelector::areaConfirmed,
            this, &Application::onAreaConfirmed);
    connect(selector, &AreaSelector::cancelled, selector, &QWidget::deleteLater);
}

void Application::onAreaConfirmed(const QRect& area, int screenIndex) {
    // Full state reset
    m_overlays->removeAll();
    m_textToOverlay.clear();
    m_textSourceRects.clear();
    m_activeTextHashes.clear();
    m_translationCache.clear();
    m_lastOcrText.clear();
    m_pendingTranslations = 0;
    m_pendingResults.clear();
    m_ocrBusy = false;
    m_lastFrameHash = 0;

    m_areas.clear();

    static int nextAreaId = 1;
    SelectionArea sel;
    sel.id          = nextAreaId++;
    sel.screenIndex = screenIndex;
    sel.geometry    = area;
    sel.enabled     = true;
    m_areas.append(sel);

    m_config->saveAreas(m_areas);
    if (m_settings) m_settings->refreshAreas();
    syncAreaOverlay();
    // Auto-trigger one snapshot after area selection so user sees immediate result
    QTimer::singleShot(300, this, &Application::onSnapshotRequested);
}

void Application::onAreaOverlayChanged(const QRect& newArea) {
    if (m_areas.isEmpty()) return;
    auto& area = m_areas.first();
    area.geometry = newArea;
    m_config->saveAreas(m_areas);
    if (m_settings) m_settings->refreshAreas();
    syncAreaOverlay();
    // Trigger re-translation after resize
    QTimer::singleShot(300, this, &Application::onSnapshotRequested);
}

void Application::onAreaEditCancelled() {
    syncAreaOverlay();
}

void Application::syncAreaOverlay() {
    if (!m_areaOverlay) return;
    if (m_areas.isEmpty()) {
        m_areaOverlay->setEditing(false);
        m_areaOverlay->hide();
        if (m_floating) m_floating->setHasArea(false);
    } else {
        const auto& area = m_areas.first();
        m_areaOverlay->setArea(area.geometry);
        // Hide persistent border after selection — re-editing uses AreaSelector
        m_areaOverlay->setEditing(false);
        m_areaOverlay->hide();
        if (m_floating) m_floating->setHasArea(true);
    }
}

void Application::onAreaCleared() {
    m_overlays->removeAll();
    m_textToOverlay.clear();
    m_textSourceRects.clear();
    m_activeTextHashes.clear();
    m_pendingTranslations = 0;
    m_ocrBusy = false;
    m_areas.clear();
    m_config->saveAreas(m_areas);
    if (m_settings) m_settings->refreshAreas();
    syncAreaOverlay();
}

void Application::onAreaEnabledChanged(int id, bool enabled) {
    for (auto& area : m_areas) {
        if (area.id == id) {
            area.enabled = enabled;
            break;
        }
    }
    m_config->saveAreas(m_areas);
}

void Application::onGlobalVisibilityToggle() {
    m_globalVisible = !m_globalVisible;
    m_tray->setGlobalVisible(m_globalVisible);
    if (m_floating) m_floating->setGlobalVisible(m_globalVisible);
    m_bus->globalVisibilityChanged(m_globalVisible);
    if (m_globalVisible) m_overlays->showAll();
    else                 m_overlays->hideAll();
}

void Application::onVisionResultReady(const QByteArray& json) {
    if (m_floating) m_floating->setPipelineStatus("idle");
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) {
        appLog("VLM: response is not a JSON array");
        return;
    }
    QJsonArray arr = doc.array();
    m_overlays->removeAll();
    m_textToOverlay.clear();
    m_activeTextHashes.clear();

    QScreen* screen = QApplication::primaryScreen();
    QRect screenBounds = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);
    int userSize = m_config->loadStyle().font.pointSize();
    QVector<QRect> placed;

    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();
        QString text = obj["text"].toString();
        QString trans = obj["translation"].toString();
        QJsonArray bbox = obj["bbox"].toArray();
        if (trans.isEmpty() || bbox.size() < 4) continue;

        QRect srcRect(bbox[0].toInt(), bbox[1].toInt(),
                      bbox[2].toInt(), bbox[3].toInt());

        // Use configured font size, not VLM bbox height
        int fontSize = userSize;

        QFont font("Microsoft YaHei", fontSize);
        QFontMetrics fm(font);
        QRect tb = fm.boundingRect(trans);
        int bubbleW = std::min(tb.width() + 24, screenBounds.width() - 10);
        int bubbleH = fm.height() + 8;

        int bubbleX = srcRect.left() + m_lastSourceRect.left();
        int bubbleY = srcRect.top() + m_lastSourceRect.top();

        // Avoid overlap: shift down if overlapping existing bubble
        QRect candidate(bubbleX, bubbleY, bubbleW, bubbleH);
        for (const auto& pr : placed) {
            if (candidate.intersects(pr)) {
                bubbleY = pr.bottom() + 2;
                candidate.moveTop(bubbleY);
            }
        }

        if (bubbleX + bubbleW > screenBounds.right())
            bubbleX = screenBounds.right() - bubbleW;
        if (bubbleY + bubbleH > screenBounds.bottom())
            bubbleY = screenBounds.bottom() - bubbleH;
        if (bubbleX < screenBounds.left()) bubbleX = screenBounds.left();
        if (bubbleY < screenBounds.top())  bubbleY = screenBounds.top();

        LayoutResult layout;
        layout.position     = QPoint(bubbleX, bubbleY);
        layout.bubbleWidth  = bubbleW;
        layout.bubbleHeight = bubbleH;
        layout.fontSize     = fontSize;
        layout.maxWidth     = bubbleW;

        int id = m_overlays->showTranslation(layout, trans);
        m_textToOverlay[qHash(text)] = id;
        placed.append(QRect(bubbleX, bubbleY, bubbleW, bubbleH));

        appLog(QString("VLM bubble: id=%1 \"%2\" pos=(%3,%4) size=%5x%6")
               .arg(id).arg(trans.left(20))
               .arg(bubbleX).arg(bubbleY).arg(bubbleW).arg(bubbleH));
    }
}

void Application::stopTranslation() {
    m_translator->cancelAll();          // abort in-flight API requests
    m_flushTimeout->stop();
    m_captureTimer->stop();
    m_overlays->removeAll();
    m_textToOverlay.clear();
    m_textSourceRects.clear();
    m_activeTextHashes.clear();
    m_pendingTranslations = 0;
    m_pendingResults.clear();
    m_batchPending.clear();
    m_isBatched = false;
    m_translatedValues.clear();
    m_lastOcrText.clear();
    m_lastFrameHash = 0;
    m_ocrBusy = false;
    if (m_floating) m_floating->setPipelineStatus("idle");
    appLog("Stop: all translations cleared");
}

void Application::switchOCREngine(const QString& name) {
    if (name == m_currentOcrEngine) return;
    stopTranslation();
    m_ocrBusy = false;

    // Disconnect old engine
    if (m_ocr) {
        disconnect(m_ocr, nullptr, this, nullptr);
        m_ocr->deleteLater();
        m_ocr = nullptr;
    }

    // Create new engine
    if (name == QStringLiteral("paddle")) {
        auto* paddle = new PaddleOCRSubprocessEngine(this);
        if (paddle->initialize("auto")) {
            m_ocr = paddle;
            m_currentOcrEngine = name;
            appLog("OCR engine switched to PaddleOCR (Python)");
        } else {
            delete paddle;
            qWarning() << "PaddleOCR subprocess init failed, falling back to Windows OCR";
            m_ocr = new WindowsOcrEngine(this);
            m_ocr->initialize("auto");
            m_currentOcrEngine = "windows";
        }
    } else {
        m_ocr = new WindowsOcrEngine(this);
        m_ocr->initialize("auto");
        m_currentOcrEngine = "windows";
        appLog("OCR engine switched to Windows OCR");
    }

    // Reconnect signals
    connect(m_ocr, &IOCREngine::recognitionComplete,
            this, &Application::onOcrCompleted);
    connect(m_ocr, &IOCREngine::recognitionError, this, [](const QString& msg) {
        appLog("OCR ERROR: " + msg);
        qWarning() << "OCR error:" << msg;
    });
}

void Application::onSettingsRequested() {
#if HAS_SETTINGS_PANEL
    if (!m_settings) {
        m_settings = new SettingsPanel(
            m_translator->plugins(),
            m_hotkey, m_config, m_bus,
            &m_areas);
        connect(m_settings, &SettingsPanel::styleChanged,
                this, &Application::onStyleChanged);
        connect(m_settings, &SettingsPanel::translatorChangeRequested,
                this, [this](const QString& name) {
            m_translator->cancelAll();
            m_pendingTranslations = 0;
            m_pendingResults.clear();
            m_flushTimeout->stop();
            m_translator->setActive(name);
            m_config->setActiveTranslator(name);
            appLog("Translator switched to: " + name);
        });
        connect(m_settings, &SettingsPanel::languageChangeRequested,
                this, &Application::onLanguageChangeRequested);
        connect(m_settings, &SettingsPanel::areaSelectRequested,
                this, &Application::launchAreaSelector);
        connect(m_settings, &SettingsPanel::areaCleared,
                this, &Application::onAreaCleared);
        connect(m_settings, &SettingsPanel::areaEnabledChanged,
                this, &Application::onAreaEnabledChanged);
    }
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
    if (m_floating) m_floating->setSettingsOpen(true);
    // Reset when settings is closed
    connect(m_settings, &QDialog::finished, this, [this]() {
        if (m_floating) m_floating->setSettingsOpen(false);
        // Hot-switch OCR engine if user changed it
        QString configured = m_config->ocrEngine();
        if (configured != m_currentOcrEngine) {
            switchOCREngine(configured);
        }
    });
#else
    qInfo() << "Settings panel not yet available (created in parallel task).";
#endif
}

void Application::onOcrCompleted(const OCRResult& result) {
    if (result.fullText.isEmpty()) {
        appLog("OCR: empty text, boxes=" + QString::number(result.boxes.size()));
        m_ocrBusy = false;
        return;
    }

    m_lastOcrText = result.fullText;
    m_pendingResults.clear();
    m_retryPerText.clear();
    m_activeGen = m_snapshotGen;  // stamp this batch with current generation

    QString srcLang = m_config->sourceLang();
    QString tgtLang = m_config->targetLang();
    QSet<int> newHashes;

    int dispatched = 0;

    if (!result.boxes.isEmpty()) {
        QRect captureRect = m_lastSourceRect;

        if (m_config->uiTranslateMode()) {
            // UI mode: light same-line grouping — merge close neighbors,
            // but keep independent UI elements (buttons, labels) separate.
            auto groups = groupUIBoxes(result.boxes, captureRect);
            for (const auto& g : groups) {
                QString text = g.text.trimmed();
                if (text.isEmpty() || m_translatedValues.contains(text)) continue;
                if (isLikelyGarbage(text)) continue;   // skip single-word noise
                if (isMostlyGarbage(text)) continue;   // skip groups of garbage words
                m_textSourceRects[text] = g.rect;
                int hash = qHash(text);
                newHashes.insert(hash);
                if (m_translationCache.contains(text)) {
                    ++m_pendingTranslations; ++dispatched;
                    onTranslationReady(text, m_translationCache[text]);
                } else {
                    m_translator->translate(text, srcLang, tgtLang);
                    ++m_pendingTranslations; ++dispatched;
                }
            }
            m_activeTextHashes = newHashes;
            goto ocrDone;
        }

        QVector<TextGroup> groups = groupTextBoxes(result.boxes, captureRect);

        // Spatial filter: drop boxes overlapping existing translation bubbles
        QVector<QRect> bubbleRects = m_overlays->existingBubbleRects();
        groups.erase(std::remove_if(groups.begin(), groups.end(),
            [this, &bubbleRects](const TextGroup& g) {
                // Text match
                if (m_translatedValues.contains(g.text)) return true;
                // Spatial overlap with any existing bubble
                for (const auto& br : bubbleRects) {
                    if (g.rect.intersects(br.adjusted(-4, -4, 4, 4)))
                        return true;
                }
                return false;
            }), groups.end());

        if (groups.isEmpty()) {
            m_ocrBusy = false;
            return;
        }

        // Filter garbage and collect valid groups
        QVector<TextGroup> validGroups;
        for (const auto& g : groups) {
            if (!isLikelyGarbage(g.text) && !isMostlyGarbage(g.text))
                validGroups.append(g);
        }
        if (validGroups.isEmpty()) {
            m_ocrBusy = false;
            return;
        }

        // Paragraph mode: merge all text preserving line breaks → one translation
        QStringList lines;
        for (const auto& g : validGroups)
            lines.append(g.text);
        QString fullText = lines.join('\n');  // preserve line structure
        QString key = fullText;
        m_textSourceRects[key] = m_lastSourceRect;  // bubble covers capture area
        int hash = qHash(key);
        newHashes.insert(hash);

        if (m_translationCache.contains(key)) {
            ++m_pendingTranslations; ++dispatched;
            onTranslationReady(key, m_translationCache[key]);
        } else {
            ++m_pendingTranslations; ++dispatched;
            m_translator->translate(fullText, srcLang, tgtLang);
        }
        m_activeTextHashes = newHashes;
    } else {
        int hash = qHash(result.fullText);
        newHashes.insert(hash);
        m_activeTextHashes = newHashes;  // before potential sync flushRowLayout
        m_textSourceRects[result.fullText] = m_lastSourceRect;

        if (m_translationCache.contains(result.fullText)) {
            ++m_pendingTranslations;
            ++dispatched;
            onTranslationReady(result.fullText, m_translationCache[result.fullText]);
        } else {
            m_translator->translate(result.fullText, srcLang, tgtLang);
            ++m_pendingTranslations;
            ++dispatched;
        }
    }

    if (dispatched > 0 && m_floating)
        m_floating->setPipelineStatus("translating");
    ocrDone:
    if (dispatched > 0)
        m_flushTimeout->start();
    m_ocrBusy = false;  // OCR done, gate on pending translations only
    appLog(QString("OCR: fullText=\"%1\" boxes=%2 groups=%3 dispatched=%4 pending=%5")
           .arg(result.fullText.left(60))
           .arg(result.boxes.size())
           .arg(newHashes.size())
           .arg(dispatched).arg(m_pendingTranslations));
}

void Application::onTranslationReady(const QString& original,
                                      const QString& translated) {
    // Selection mode active → all translations go to popup, skip overlay bubbles
    if (m_selectionMode) {
        if (m_pendingTranslations > 0) --m_pendingTranslations;
        return;
    }

    // Handle numbered batch response: per-segment validation + selective retry
    if (m_isBatched && !m_batchPending.isEmpty()) {
        if (m_pendingTranslations > 0) --m_pendingTranslations;

        // Parse response: prefer JSON object with "translations" array
        QStringList parts;
        QJsonDocument doc = QJsonDocument::fromJson(translated.trimmed().toUtf8());
        if (doc.isObject() && doc.object().contains("translations")) {
            for (const auto& v : doc.object()["translations"].toArray())
                parts.append(v.toString());
        } else if (doc.isArray()) {
            for (const auto& v : doc.array())
                parts.append(v.toString());
        } else {
            parts = translated.trimmed().split(QStringLiteral("|-|"));
        }

        int goodCount = 0, retried = 0;
        for (int i = 0; i < m_batchPending.size(); ++i) {
            QString seg = (i < parts.size()) ? parts[i].trimmed() : QString();
            QRegularExpression numPrefix(R"(^\[\d+\]\s*)");
            seg.remove(numPrefix);

            QString orig = m_batchPending[i].first;
            QRect srcRect = m_batchPending[i].second;

            // Per-segment validation
            bool bad = false;
            if (seg.isEmpty() || seg == "??" || seg == "???")         bad = true;
            else if (seg == orig)                                       bad = true;
            else if (seg.size() > orig.size() * 3 && seg.size() > 30)  bad = true;
            else if (seg.contains(orig) && seg != orig)                 bad = true;
            else {
                int sym = 0, alpha = 0;
                for (const QChar& c : seg) {
                    if (c.isLetterOrNumber()) ++alpha; else if (!c.isSpace()) ++sym;
                }
                if (alpha == 0 && sym > 0) bad = true;
            }

            if (bad && m_retryPerText.value(orig, 0) < 3) {
                // Self-healing: retry just this one segment individually
                ++m_retryPerText[orig];
                ++m_pendingTranslations;
                m_translator->translate(orig,
                    m_config->sourceLang(), m_config->targetLang());
                ++retried;
                appLog(QString("Batch seg[%1] RETRY").arg(i));
            } else if (bad) {
                // Exhausted retries → give up
                m_pendingResults.append({orig, QStringLiteral("??"), srcRect});
                m_translationCache[orig] = QStringLiteral("??");
            } else {
                // Good → show immediately
                m_pendingResults.append({orig, seg, srcRect});
                m_translationCache[orig] = seg;
                ++goodCount;
            }
        }

        if (goodCount > 0) flushRowLayout();
        m_isBatched = (retried > 0);
        if (!m_isBatched) m_batchPending.clear();
        return;
    }

    // Reject stale responses from previous snapshots
    if (m_activeGen != m_snapshotGen) {
        appLog(QString("TransReady: STALE (gen %1 != %2)").arg(m_activeGen).arg(m_snapshotGen));
        return;
    }

    appLog(QString("TransReady: orig=\"%1\" trans=\"%2\" vis=%3 pending=%4")
           .arg(original.left(40), translated.left(40))
           .arg(m_globalVisible).arg(m_pendingTranslations));

    // Extract translation from JSON if model output is structured
    QString t = translated.trimmed();
    QJsonDocument jdoc = QJsonDocument::fromJson(t.toUtf8());
    if (jdoc.isObject()) {
        QJsonObject obj = jdoc.object();
        if (obj.contains("translation"))
            t = obj["translation"].toString().trimmed();
        else if (obj.contains("translations") && obj["translations"].isArray())
            t = obj["translations"].toVariant().toStringList().join(QStringLiteral("|-|"));
    }

    // Strip common LLM verbosity prefixes
    static const QStringList noisePrefixes = {
        "Translation:", "The translation is:", "Translated text:",
        "Here is the translation:", "Output:", "Result:",
        "翻译：", "译文：", "翻译结果：", "翻译如下：",
    };
    for (const auto& prefix : noisePrefixes) {
        if (t.startsWith(prefix, Qt::CaseInsensitive))
            t = t.mid(prefix.length()).trimmed();
    }
    // Strip surrounding quotes
    if (t.size() >= 2 && ((t.startsWith('"') && t.endsWith('"'))
                       || (t.startsWith('\'') && t.endsWith('\''))
                       || (t.startsWith("「") && t.endsWith("」"))))
        t = t.mid(1, t.size() - 2).trimmed();

    // Strip trailing notes: parenthetical OR standalone sentences at end
    static const QRegularExpression trailNote(
        R"(\s*\((?:Note|注|备注|注释)[：:].*\)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    t.remove(trailNote);

    // Strip standalone trailing note lines (model refuses to shut up)
    QStringList lines = t.split('\n');
    static const QRegularExpression noteLine(
        R"(^(?:注意|注|备注|注释|Note|Disclaimer)[：:])",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression disclaimerLine(
        R"(^(?:本翻译|This translation|仅供参考|for reference|请勿).*$)",
        QRegularExpression::CaseInsensitiveOption);
    while (!lines.isEmpty()) {
        QString last = lines.last().trimmed();
        if (last.isEmpty() || noteLine.match(last).hasMatch()
                         || disclaimerLine.match(last).hasMatch())
            lines.removeLast();
        else
            break;
    }
    if (lines.size() < t.split('\n').size())  // something was stripped
        t = lines.join('\n').trimmed();

    // Detect failed translations — identify the specific reason for self-healing
    bool isFailed = false;
    QString failReason;
    if (t.isEmpty())                           { isFailed = true; failReason = "empty response"; }
    else if (t == "?" || t == "??" || t == "???") { isFailed = true; failReason = "only question marks"; }
    else if (t == original.trimmed()) {
        // If original is already target language, model is right — accept it
        QString tgtLang = m_config->targetLang();
        bool hasCJK = original.contains(QRegularExpression("\\p{Han}"));
        bool allAscii = original.contains(QRegularExpression("^[a-zA-Z0-9\\s]+$"));
        appLog(QString("Validate: t==orig [%1] tgt=[%2] hasCJK=%3 allAscii=%4")
               .arg(original.left(30), tgtLang)
               .arg(hasCJK).arg(allAscii));
        if (tgtLang == "zh" && hasCJK)
            { /* accept, not a failure */ }
        else if (tgtLang == "en" && allAscii)
            { /* accept */ }
        else
            { isFailed = true; failReason = "same as original"; }
    }
    else {
        int sym = 0, alpha = 0;
        for (const QChar& c : t) {
            if (c.isLetterOrNumber()) ++alpha; else if (!c.isSpace()) ++sym;
        }
        if (alpha == 0 && sym > 0)             { isFailed = true; failReason = "pure symbols"; }
        else if (t.size() <= 3 && sym >= t.size()) { isFailed = true; failReason = "too short, all symbols"; }
        else if (t.size() > original.trimmed().size() * 3 && t.size() > 30)
                                                { isFailed = true; failReason = "too long (likely concatenated)"; }
        else if (t.contains(original.trimmed()) && t != original.trimmed())
                                                { isFailed = true; failReason = "contains original text (echo)"; }
    }

    if (isFailed) {
        int& tries = m_retryPerText[original.trimmed()];
        if (++tries > 3) {
            // Too many retries for this text — give up and show "??" as-is
            if (m_pendingTranslations > 0) --m_pendingTranslations;
            if (m_globalVisible) {
                QRect sourceRect = m_textSourceRects.value(original);
                if (sourceRect.isNull()) sourceRect = m_lastSourceRect;
                m_pendingResults.append({original, QStringLiteral("??"), sourceRect});
            }
            flushRowLayout();
            return;
        }
        appLog(QString("TransReady: RETRY #%1 \"%2\" reason=%3")
               .arg(tries).arg(original.left(30), failReason));
        // Clean retry: re-send original text. Model sees fresh context without noise.
        ++m_pendingTranslations;
        m_translator->translate(original.trimmed(), m_config->sourceLang(),
                                m_config->targetLang());
        return;
    }

    if (m_pendingTranslations > 0) --m_pendingTranslations;

    if (m_globalVisible && !t.isEmpty()) {
        QRect sourceRect = m_textSourceRects.value(original);
        if (sourceRect.isNull())
            sourceRect = m_lastSourceRect;
        m_pendingResults.append({original, t, sourceRect});
        m_translationCache[original] = t;
    } else {
        appLog(QString("TransReady: SKIPPED vis=%1 empty=%2")
               .arg(m_globalVisible).arg(t.isEmpty()));
    }

    // Show each translation bubble as soon as it arrives — don't wait for all
    flushRowLayout();
}

void Application::flushRowLayout() {
    m_flushTimeout->stop();
    if (m_pendingResults.isEmpty()) {
        if (m_floating) m_floating->setPipelineStatus("idle");
        return;
    }

    QScreen* screen = QApplication::primaryScreen();
    QRect screenBounds = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);

    // Sort top-to-bottom so lower source text gets placed after upper
    std::sort(m_pendingResults.begin(), m_pendingResults.end(),
              [](const Pending& a, const Pending& b) {
                  return a.sourceRect.top() < b.sourceRect.top();
              });

    QVector<QRect> placed;  // track to avoid overlap
    bool uiMode = m_config->uiTranslateMode();

    // Compute items-per-row for grid density detection
    QHash<int, int> rowItemCount;  // rounded Y → count
    const int kRowBucket = 16;     // bucket size for "same row"
    for (const auto& p : m_pendingResults) {
        int bucket = (p.sourceRect.top() + kRowBucket/2) / kRowBucket;
        ++rowItemCount[bucket];
    }

    for (const auto& r : m_pendingResults) {
        LayoutResult layout;
        bool isParagraph = !uiMode && m_pendingResults.size() == 1
                        && r.sourceRect.width() > 200;

        if (isParagraph) {
            // Paragraph mode: overlay on original text, same size as capture area
            int fontSize = m_config->loadStyle().font.pointSize();
            layout.position     = r.sourceRect.topLeft();
            layout.bubbleWidth  = r.sourceRect.width();
            layout.bubbleHeight = r.sourceRect.height();
            layout.fontSize     = fontSize;
            layout.maxWidth     = r.sourceRect.width();
            layout.wordWrap     = true;
        } else if (uiMode) {
            // UI mode: overlay bubble directly on top of original text element.
            // This covers the source text so the user sees only the translation.
            int baseFontSize = m_config->loadStyle().font.pointSize();
            QFont font("Microsoft YaHei", baseFontSize);
            QFontMetrics fm(font);
            int textW = fm.horizontalAdvance(r.translated);
            int textH = fm.height();

            // Use source rect width if the translation fits; otherwise expand
            int srcW = r.sourceRect.width();
            int srcH = r.sourceRect.height();
            int bubbleW = qMax(srcW, textW + 12);
            int bubbleH = qMax(srcH, textH + 10);

            // Shrink font if translation is too wide for source rect
            int fontSize = baseFontSize;
            while (fontSize > 8 && textW > srcW + 20) {
                --fontSize;
                QFont f("Microsoft YaHei", fontSize);
                QFontMetrics fm2(f);
                textW = fm2.horizontalAdvance(r.translated);
                bubbleW = qMax(srcW, textW + 12);
            }

            layout.position     = r.sourceRect.topLeft();
            layout.bubbleWidth  = bubbleW;
            layout.bubbleHeight = bubbleH;
            layout.fontSize     = fontSize;
            layout.maxWidth     = bubbleW;
            layout.wordWrap     = false;  // single-line for UI elements
        } else {
            LayoutRequest req;
            req.sourceRect       = r.sourceRect;
            req.translatedText   = r.translated;
            req.preferredFontSize = m_config->loadStyle().font.pointSize();
            layout = m_layout->compute(req, placed, screenBounds);
        }

        int bubbleW = layout.bubbleWidth;
        int bubbleH = layout.bubbleHeight;
        QPoint bubblePos = layout.position;

        int textHash = qHash(r.original);
        m_translationCache[r.original] = r.translated;
        m_translatedValues.insert(r.translated);

        if (m_textToOverlay.contains(textHash)) {
            m_overlays->updateTranslation(m_textToOverlay[textHash], r.translated, layout);
        } else {
            int id = m_overlays->showTranslation(layout, r.translated);
            m_textToOverlay[textHash] = id;
        }

        appLog(QString("Inline: id=%1 \"%2\" pos=(%3,%4) size=%5x%6 wrap=%7")
               .arg(m_textToOverlay[textHash])
               .arg(r.original.left(30))
               .arg(bubblePos.x()).arg(bubblePos.y())
               .arg(bubbleW).arg(bubbleH)
               .arg(layout.wordWrap));

        placed.append(QRect(bubblePos, QSize(bubbleW, bubbleH)));
    }

    // Stale cleanup (RealTime only)
    if (m_mode == Mode::RealTime) {
        QList<int> staleIds;
        for (auto it = m_textToOverlay.begin(); it != m_textToOverlay.end(); ++it) {
            if (!m_activeTextHashes.contains(it.key()))
                staleIds.append(it.value());
        }
        for (int id : staleIds) {
            m_overlays->removeTranslation(id);
            for (auto it = m_textToOverlay.begin(); it != m_textToOverlay.end(); ) {
                if (it.value() == id)
                    it = m_textToOverlay.erase(it);
                else
                    ++it;
            }
        }
    }

    if (m_floating) m_floating->setPipelineStatus("done");
    // Reset to idle after a brief moment
    QTimer::singleShot(1200, this, [this]() {
        if (m_floating) m_floating->setPipelineStatus("idle");
    });

    m_pendingResults.clear();
}

void Application::onStyleChanged(const StyleConfig& style) {
    m_overlays->updateAllStyles(style);
}

void Application::onLanguageChangeRequested(const QString& lang) {
    LanguageManager::instance()->switchLanguage(lang);
    m_config->setTranslatorConfig("app", "language", lang);
    m_tray->retranslateUi();
    if (m_floating) m_floating->retranslateUi();
}

void Application::toggleSelectionMode() {
    m_selectionMode = !m_selectionMode;
    if (m_selectionMode) {
        // Stop area-based translation to avoid concurrency
        stopTranslation();
        m_captureTimer->stop();
        m_selMonitor->start();
    } else {
        m_selMonitor->stop();
        // Resume area-based translation if it was running
        if (m_mode == Mode::RealTime)
            m_captureTimer->start();
    }
    if (m_floating) m_floating->setSelectionMode(m_selectionMode);
    if (m_tray)    m_tray->setSelectionMode(m_selectionMode);
    appLog(QString("Selection mode: %1").arg(m_selectionMode ? "ON" : "OFF"));
}

void Application::onTextSelected(const QString& text, QPoint cursorPos) {
    if (!m_selectionMode) return;
    if (text.trimmed().isEmpty()) return;

    appLog(QString("Selection: \"%1\" at (%2,%3)")
           .arg(text.left(60), QString::number(cursorPos.x()),
                QString::number(cursorPos.y())));

    // Show "translating..." placeholder immediately
    m_selPopup->showTranslation(cursorPos, text.trimmed(),
                                QStringLiteral("翻译中..."));

    // Signal main handler to skip overlay bubbles for this text
    m_pendingSelectionText = text.trimmed();

    // Translate using active translator
    m_translator->translate(text.trimmed(),
        m_config->sourceLang(), m_config->targetLang());

    // Single-shot: update popup when translation arrives
    QMetaObject::Connection* conn = new QMetaObject::Connection;
    *conn = connect(m_translator, &TranslatorManager::translationReady, this,
        [this, text, cursorPos, conn](const QString& orig, const QString& trans) {
            if (orig.trimmed() == text.trimmed()) {
                // Extract translation from JSON
                QString result = trans.trimmed();
                QJsonDocument jd = QJsonDocument::fromJson(result.toUtf8());
                if (jd.isObject() && jd.object().contains("translation"))
                    result = jd.object()["translation"].toString().trimmed();
                m_selPopup->showTranslation(cursorPos, orig.trimmed(), result);
                disconnect(*conn);
                delete conn;
            }
        });
}
