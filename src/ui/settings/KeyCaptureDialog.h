#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtGui/QKeySequence>

class KeyCaptureDialog : public QDialog {
    Q_OBJECT

public:
    explicit KeyCaptureDialog(const QString& currentKeys, QWidget* parent = nullptr);

    QString capturedKeys() const { return m_capturedKeys; }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    QString modifiersToString(Qt::KeyboardModifiers mods) const;
    QString keyToString(int qtKey) const;

    QLabel* m_label = nullptr;
    QString m_capturedKeys;
    int     m_pressedKeys = 0;
};
