#pragma once

#include <QtCore/QObject>
#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QVector>
#include <QtCore/QHash>
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
    QHash<QString, int>   m_registeredIds;
    QHash<int, QString>   m_idToBinding;
    int m_nextId = 1;
};
