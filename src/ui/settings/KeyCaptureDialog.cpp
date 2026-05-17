#include "KeyCaptureDialog.h"
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QKeyEvent>
#include <QtCore/QTimer>

KeyCaptureDialog::KeyCaptureDialog(const QString& currentKeys, QWidget* parent)
    : QDialog(parent), m_originalKeys(currentKeys) {
    setWindowTitle(tr("Capture Shortcut"));
    setFixedSize(380, 180);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch();

    m_label = new QLabel(tr("Current: %1\n\nPress new shortcut keys...\n(Esc to cancel, Backspace to clear)")
                          .arg(currentKeys));
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    layout->addWidget(m_label);
    layout->addStretch();

    installEventFilter(this);
    setFocus();
}

bool KeyCaptureDialog::eventFilter(QObject* obj, QEvent* event) {
    if (obj != this) return QDialog::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
        if (ke->key() == Qt::Key_Backspace) {
            m_capturedKeys.clear();
            m_readyToAccept = false;
            m_label->setText(tr("Current: %1\n\nCleared. Press new keys...\n(Esc to cancel)")
                              .arg(m_originalKeys));
            return true;
        }
        onKeyPressed(ke->key(), ke->modifiers());
        return true;
    }

    if (event->type() == QEvent::KeyRelease) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (!ke->isAutoRepeat() && m_readyToAccept && !m_capturedKeys.isEmpty()) {
            QTimer::singleShot(50, this, &QDialog::accept);
        }
        return true;
    }

    return QDialog::eventFilter(obj, event);
}

void KeyCaptureDialog::onKeyPressed(int qtKey, Qt::KeyboardModifiers mods) {
    // Ignore standalone modifier presses
    switch (qtKey) {
    case Qt::Key_Control: case Qt::Key_Shift:
    case Qt::Key_Alt:     case Qt::Key_Meta:
        return;
    default: break;
    }

    QString modStr = modifiersToString(mods);
    QString keyStr = keyToString(qtKey);
    if (keyStr.isEmpty()) return;

    m_capturedKeys = modStr.isEmpty() ? keyStr : modStr + "+" + keyStr;
    m_readyToAccept = true;

    m_label->setText(tr("Current: %1\n\nNew: %2\n(Release keys to confirm, Esc to cancel)")
                      .arg(m_originalKeys, m_capturedKeys));
}

QString KeyCaptureDialog::modifiersToString(Qt::KeyboardModifiers mods) const {
    QStringList parts;
    if (mods & Qt::ControlModifier) parts << "Ctrl";
    if (mods & Qt::ShiftModifier)   parts << "Shift";
    if (mods & Qt::AltModifier)     parts << "Alt";
    if (mods & Qt::MetaModifier)    parts << "Win";
    return parts.join("+");
}

QString KeyCaptureDialog::keyToString(int qtKey) const {
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return QChar('A' + (qtKey - Qt::Key_A));
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return QChar('0' + (qtKey - Qt::Key_0));
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
        return QString("F%1").arg(qtKey - Qt::Key_F1 + 1);

    switch (qtKey) {
    case Qt::Key_Space:        return "Space";
    case Qt::Key_Tab:          return "Tab";
    case Qt::Key_Backspace:    return "Backspace";
    case Qt::Key_Return:
    case Qt::Key_Enter:        return "Enter";
    case Qt::Key_Up:           return "Up";
    case Qt::Key_Down:         return "Down";
    case Qt::Key_Left:         return "Left";
    case Qt::Key_Right:        return "Right";
    case Qt::Key_Home:         return "Home";
    case Qt::Key_End:          return "End";
    case Qt::Key_PageUp:       return "PageUp";
    case Qt::Key_PageDown:     return "PageDown";
    case Qt::Key_Insert:       return "Insert";
    case Qt::Key_Delete:       return "Delete";
    case Qt::Key_Escape:       return "Esc";
    case Qt::Key_Print:        return "PrintScreen";
    case Qt::Key_Pause:        return "Pause";
    case Qt::Key_Minus:        return "-";
    case Qt::Key_Equal:        return "=";
    case Qt::Key_BracketLeft:  return "[";
    case Qt::Key_BracketRight: return "]";
    case Qt::Key_Semicolon:    return ";";
    case Qt::Key_Apostrophe:   return "'";
    case Qt::Key_Comma:        return ",";
    case Qt::Key_Period:       return ".";
    case Qt::Key_Slash:        return "/";
    case Qt::Key_Backslash:    return "\\";
    case Qt::Key_QuoteLeft:    return "`";
    default: return {};
    }
}
