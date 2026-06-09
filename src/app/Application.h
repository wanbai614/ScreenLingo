#pragma once

#include <QtWidgets/QApplication>
#include <QtCore/QTimer>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QVector>
#include <memory>
#include "common/Types.h"

class SignalBus;
class DxgiCaptureEngine;
class IOCREngine;
class TranslatorManager;
class LayoutEngine;
class OverlayManager;
class TrayManager;
class HotkeyManager;
class FloatingToolbar;
class AreaOverlay;
class SettingsPanel;
class Config;
class MouseSelectionMonitor;
class TranslationPopup;
class SubtitleOverlay;

class Application : public QObject {
    Q_OBJECT

public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    bool initialize();

private slots:
    void processRealtimeFrame();
    void onHotkeyTriggered(const QString& id);
    void onModeChanged(Mode mode);
    void onSnapshotRequested();
    void onAreaConfirmed(const QRect& area, int screenIndex);
    void onAreaOverlayChanged(const QRect& newArea);
    void onAreaEditCancelled();
    void onAreaCleared();
    void onAreaEnabledChanged(int id, bool enabled);
    void onGlobalVisibilityToggle();
    void onSettingsRequested();
    void onOcrCompleted(const OCRResult& result);
    void onTranslationReady(const QString& original, const QString& translated);
    void onStyleChanged(const StyleConfig& style);
    void onLanguageChangeRequested(const QString& lang);
    void stopTranslation();
    void onVisionResultReady(const QByteArray& json);
    void switchOCREngine(const QString& name);
    void toggleSelectionMode();
    void onTextSelected(const QString& text, QPoint cursorPos);

private:
    void setMode(Mode mode);
    void flushRowLayout();
    void syncAreaOverlay();
    void launchAreaSelector();

    // Core
    SignalBus*          m_bus         = nullptr;
    DxgiCaptureEngine*  m_capture     = nullptr;
    IOCREngine*         m_ocr         = nullptr;
    QString              m_currentOcrEngine;
    TranslatorManager*  m_translator  = nullptr;
    LayoutEngine*       m_layout      = nullptr;

    // UI
    OverlayManager*     m_overlays    = nullptr;
    TrayManager*        m_tray        = nullptr;
    HotkeyManager*      m_hotkey      = nullptr;
    FloatingToolbar*    m_floating    = nullptr;
    AreaOverlay*        m_areaOverlay = nullptr;
    SettingsPanel*      m_settings    = nullptr;
    Config*             m_config      = nullptr;
    MouseSelectionMonitor* m_selMonitor = nullptr;
    TranslationPopup*      m_selPopup   = nullptr;
    SubtitleOverlay*    m_subtitleOverlay = nullptr;

    // State
    Mode                    m_mode = Mode::Snapshot;
    QVector<SelectionArea>  m_areas;
    QTimer*                 m_captureTimer = nullptr;
    bool                    m_globalVisible = true;
    bool                    m_selectionMode = false;
    bool                    m_dragMode      = false;

    // Tracking: source rect of the last OCR/capture request,
    // used when translation completes since TranslatorManager signal
    // does not carry a QRect.
    QRect               m_lastSourceRect;

    // Map text hash → overlay id for update-on-re-translation
    QHash<int, int>     m_textToOverlay;

    // Track which hashes are active this cycle (real-time mode cleanup)
    QSet<int>           m_activeTextHashes;

    // Map original text → screen position of source
    QHash<QString, QRect> m_textSourceRects;

    // Prevent concurrent OCR/translation cycles
    bool m_ocrBusy            = false;
    int  m_pendingTranslations = 0;
    int  m_snapshotGen = 0;     // generation counter — reject stale responses
    int  m_activeGen = 0;       // generation of current in-flight batch
    int  m_subtitleGen = 0;     // generation counter for subtitle translations
    QTimer* m_flushTimeout = nullptr;

    // Translation cache: original text → translated text (avoid re-translating)
    QHash<QString, QString> m_translationCache;
    QSet<QString> m_translatedValues;  // quick lookup to skip OCR re-reading bubbles
    QHash<QString, int> m_retryPerText; // per-text retry counter (reset each OCR cycle)
    QString m_pendingSelectionText;    // text currently being translated for selection popup

    // Batch translation: batched text → list of (individual text, source rect)
    QHash<QString, QVector<QPair<QString, QRect>>> m_batchMap;
    // Track which batch each original text belongs to (for cleanup)
    QHash<QString, QString> m_textToBatchKey;

    // Last OCR full text (avoid re-processing identical frames)
    QString m_lastOcrText;
    QString m_lastSubtitleText;

    // Pixel-level frame dedup (more reliable than OCR text compare)
    uint m_lastFrameHash = 0;

    // Row-layout: accumulate results then flush together
    struct Pending {
        QString original;
        QString translated;
        QRect   sourceRect;
    };
    QVector<Pending> m_pendingResults;
};
