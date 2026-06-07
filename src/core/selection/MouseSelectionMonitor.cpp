#include "MouseSelectionMonitor.h"
#include <QtCore/QDebug>

MouseSelectionMonitor* MouseSelectionMonitor::s_instance = nullptr;

MouseSelectionMonitor::MouseSelectionMonitor(QObject* parent)
    : QObject(parent) {}

MouseSelectionMonitor::~MouseSelectionMonitor() {
    stop();
    s_instance = nullptr;
}

void MouseSelectionMonitor::start() {
    if (m_active) return;
    s_instance = this;
    m_hook = SetWindowsHookExW(WH_MOUSE_LL, hookProc,
        GetModuleHandleW(nullptr), 0);
    if (m_hook) {
        m_active = true;
        qDebug() << "[SelMonitor] Hook installed";
    } else {
        qWarning() << "[SelMonitor] Failed to install mouse hook:" << GetLastError();
        s_instance = nullptr;
    }
}

void MouseSelectionMonitor::stop() {
    if (!m_active) return;
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    m_active   = false;
    m_dragging = false;
    s_instance = nullptr;
    qDebug() << "[SelMonitor] Hook removed";
}

bool MouseSelectionMonitor::isActive() const { return m_active; }

LRESULT CALLBACK MouseSelectionMonitor::hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || !s_instance)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

    switch (wParam) {
    case WM_LBUTTONDOWN:
        s_instance->m_downPos  = ms->pt;
        s_instance->m_dragging = false;
        break;

    case WM_MOUSEMOVE:
        if (!s_instance->m_dragging) {
            int dx = ms->pt.x - s_instance->m_downPos.x;
            int dy = ms->pt.y - s_instance->m_downPos.y;
            if (abs(dx) > 8 || abs(dy) > 8)
                s_instance->m_dragging = true;
        }
        break;

    case WM_LBUTTONUP:
        if (s_instance->m_dragging) {
            s_instance->m_dragging = false;
            // Defer to next message-loop iteration so the selection is
            // finalised in the target app before we send Ctrl+C.
            QMetaObject::invokeMethod(s_instance,
                [cursorPos = ms->pt]() {
                    if (s_instance)
                        s_instance->handleMouseUp();
                }, Qt::QueuedConnection);
        }
        break;
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void MouseSelectionMonitor::handleMouseUp() {
    if (!m_active) return;

    // Ignore if foreground window is our own process
    HWND fg = GetForegroundWindow();
    if (fg) {
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        if (pid == GetCurrentProcessId()) return;
    }

    QString text = readSelectedText();
    if (text.isEmpty()) return;

    // Trim to reasonable length
    text = text.trimmed();
    if (text.length() > 2000)
        text = text.left(2000) + QStringLiteral("...");

    // Emit with current cursor position
    POINT pt;
    GetCursorPos(&pt);
    emit textSelected(text, QPoint(pt.x, pt.y));
}

QString MouseSelectionMonitor::readSelectedText() {
    // --- Save current clipboard ---
    QString previous;
    if (OpenClipboard(nullptr)) {
        if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            HANDLE h = GetClipboardData(CF_UNICODETEXT);
            if (h) {
                auto* wstr = static_cast<const wchar_t*>(GlobalLock(h));
                if (wstr) {
                    previous = QString::fromWCharArray(wstr);
                    GlobalUnlock(h);
                }
            }
        }
        CloseClipboard();
    }

    // --- Simulate Ctrl+C ---
    // keybd_event is simplest and reliable; SendInput is also fine.
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'C';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'C';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));

    // Wait for clipboard to update
    Sleep(50);

    // --- Read new clipboard ---
    QString selected;
    if (OpenClipboard(nullptr)) {
        if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            HANDLE h = GetClipboardData(CF_UNICODETEXT);
            if (h) {
                auto* wstr = static_cast<const wchar_t*>(GlobalLock(h));
                if (wstr) {
                    selected = QString::fromWCharArray(wstr);
                    GlobalUnlock(h);
                }
            }
        }
        CloseClipboard();
    }

    // --- Restore previous clipboard ---
    if (!previous.isEmpty()) {
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            size_t sz = (previous.size() + 1) * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sz);
            if (hMem) {
                auto* dst = static_cast<wchar_t*>(GlobalLock(hMem));
                if (dst) {
                    wcscpy_s(dst, previous.size() + 1,
                        reinterpret_cast<const wchar_t*>(previous.utf16()));
                    GlobalUnlock(hMem);
                }
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
        }
    }

    // Return only if different from previous (new text)
    if (selected == previous) return {};
    return selected;
}
