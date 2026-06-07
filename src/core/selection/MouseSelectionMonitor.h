#pragma once

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QString>
#include <Windows.h>

/// Global low-level mouse hook that detects left-button drag-and-release
/// (text selection gesture) then retrieves the selected text via Ctrl+C.
class MouseSelectionMonitor : public QObject {
    Q_OBJECT

public:
    explicit MouseSelectionMonitor(QObject* parent = nullptr);
    ~MouseSelectionMonitor() override;

    void start();
    void stop();
    bool isActive() const;

signals:
    /// Emitted when text selection is detected and copied from clipboard.
    /// @param text  The selected text (empty if nothing was selected).
    /// @param cursorScreenPos  Cursor position at release time (screen coords).
    void textSelected(const QString& text, QPoint cursorScreenPos);

private:
    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);
    void handleMouseUp();
    static QString readSelectedText();

    HHOOK  m_hook        = nullptr;
    POINT  m_downPos     = {0, 0};
    bool   m_dragging    = false;
    bool   m_active      = false;

    static MouseSelectionMonitor* s_instance;
};
