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
            QKeySequence ks(p);
            if (!ks.isEmpty()) {
                vk = ks[0].toCombined() & 0x01FF;
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
