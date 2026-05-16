#pragma once

#include <QtWidgets/QApplication>
#include <QtCore/QTimer>
#include <QtCore/QHash>
#include <QtCore/QSet>
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

    bool initialize();

private slots:
    void processRealtimeFrame();
    void onHotkeyTriggered(const QString& id);
    void onModeChanged(Mode mode);
    void onSnapshotRequested();
    void onAreaConfirmed(const QRect& area, int screenIndex);
    void onAreaCleared();
    void onAreaEnabledChanged(int id, bool enabled);
    void onGlobalVisibilityToggle();
    void onSettingsRequested();
    void onOcrCompleted(const OCRResult& result);
    void onTranslationReady(const QString& original, const QString& translated);
    void onStyleChanged(const StyleConfig& style);
    void onLanguageChangeRequested(const QString& lang);

private:
    void setMode(Mode mode);

    // Core
    SignalBus*          m_bus         = nullptr;
    DxgiCaptureEngine*  m_capture     = nullptr;
    WindowsOcrEngine*   m_ocr         = nullptr;
    TranslatorManager*  m_translator  = nullptr;
    LayoutEngine*       m_layout      = nullptr;

    // UI
    OverlayManager*     m_overlays    = nullptr;
    TrayManager*        m_tray        = nullptr;
    HotkeyManager*      m_hotkey      = nullptr;
    SettingsPanel*      m_settings    = nullptr;
    Config*             m_config      = nullptr;

    // State
    Mode                    m_mode = Mode::Snapshot;
    QVector<SelectionArea>  m_areas;
    QTimer*                 m_captureTimer = nullptr;
    bool                    m_globalVisible = true;

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
};
