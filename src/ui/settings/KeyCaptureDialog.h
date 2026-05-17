#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtCore/QEvent>

class KeyCaptureDialog : public QDialog {
    Q_OBJECT

public:
    explicit KeyCaptureDialog(const QString& currentKeys, QWidget* parent = nullptr);

    QString capturedKeys() const { return m_capturedKeys; }

    bool eventFilter(QObject* obj, QEvent* event) override;

    void onKeyPressed(int qtKey, Qt::KeyboardModifiers mods);
    QString modifiersToString(Qt::KeyboardModifiers mods) const;
    QString keyToString(int qtKey) const;

    QLabel* m_label = nullptr;
    QString m_capturedKeys;
    QString m_originalKeys;
    bool    m_readyToAccept = false;
};
