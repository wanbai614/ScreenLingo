#include "KeyCaptureDialog.h"
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QKeyEvent>

KeyCaptureDialog::KeyCaptureDialog(const QString& currentKeys, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Capture Shortcut"));
    setFixedSize(340, 160);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch();

    m_label = new QLabel(tr("Current: %1\n\nPress new shortcut keys...\n(Esc to cancel)")
                          .arg(currentKeys));
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    layout->addWidget(m_label);
    layout->addStretch();

    setFocusPolicy(Qt::StrongFocus);
    grabKeyboard();
}

void KeyCaptureDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        releaseKeyboard();
        reject();
        return;
    }

    // Ignore standalone modifier keys
    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift ||
        event->key() == Qt::Key_Alt   || event->key() == Qt::Key_Meta) {
        QDialog::keyPressEvent(event);
        return;
    }

    ++m_pressedKeys;

    Qt::KeyboardModifiers mods = event->modifiers();
    int qtKey = event->key();

    QString modStr = modifiersToString(mods);
    QString keyStr = keyToString(qtKey);

    if (keyStr.isEmpty()) {
        QDialog::keyPressEvent(event);
        return;
    }

    m_capturedKeys = modStr.isEmpty() ? keyStr : modStr + "+" + keyStr;

    m_label->setText(tr("Current: %1\n\nNew: %2\n(Press Esc to cancel)")
                      .arg(m_label->text().section('\n', 0, 0)
                               .remove("Current: "),
                           m_capturedKeys));
}

void KeyCaptureDialog::keyReleaseEvent(QKeyEvent* event) {
    Q_UNUSED(event);
    --m_pressedKeys;

    if (m_pressedKeys <= 0 && !m_capturedKeys.isEmpty()) {
        releaseKeyboard();
        accept();
    }
}

QString KeyCaptureDialog::modifiersToString(Qt::KeyboardModifiers mods) const {
    QStringList parts;
    if (mods & Qt::ControlModifier) parts << "Ctrl";
    if (mods & Qt::ShiftModifier)   parts << "Shift";
    if (mods & Qt::AltModifier)      parts << "Alt";
    if (mods & Qt::MetaModifier)     parts << "Win";
    return parts.join("+");
}

QString KeyCaptureDialog::keyToString(int qtKey) const {
    // Letters
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return QChar(qtKey);
    // Numbers
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return QChar(qtKey);
    // Function keys
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
        return QString("F%1").arg(qtKey - Qt::Key_F1 + 1);

    switch (qtKey) {
    case Qt::Key_Space:       return "Space";
    case Qt::Key_Tab:         return "Tab";
    case Qt::Key_Backspace:   return "Backspace";
    case Qt::Key_Return:
    case Qt::Key_Enter:       return "Enter";
    case Qt::Key_Up:          return "Up";
    case Qt::Key_Down:        return "Down";
    case Qt::Key_Left:        return "Left";
    case Qt::Key_Right:       return "Right";
    case Qt::Key_Home:        return "Home";
    case Qt::Key_End:         return "End";
    case Qt::Key_PageUp:      return "PageUp";
    case Qt::Key_PageDown:    return "PageDown";
    case Qt::Key_Insert:      return "Insert";
    case Qt::Key_Delete:      return "Delete";
    case Qt::Key_Escape:      return "Esc";
    case Qt::Key_Print:       return "PrintScreen";
    case Qt::Key_Pause:       return "Pause";
    case Qt::Key_Minus:       return "-";
    case Qt::Key_Equal:       return "=";
    case Qt::Key_BracketLeft: return "[";
    case Qt::Key_BracketRight:return "]";
    case Qt::Key_Semicolon:   return ";";
    case Qt::Key_Apostrophe:  return "'";
    case Qt::Key_Comma:       return ",";
    case Qt::Key_Period:      return ".";
    case Qt::Key_Slash:       return "/";
    case Qt::Key_Backslash:   return "\\";
    case Qt::Key_QuoteLeft:    return "`";
    default: return {};
    }
}
