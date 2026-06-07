#include "TranslationOverlay.h"
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtWidgets/QApplication>
#include <QtGui/QClipboard>
#include <QtCore/QTimer>
#include <QtGui/QMouseEvent>
#define NOMINMAX
#include <windows.h>

TranslationOverlay::TranslationOverlay(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
                   Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
}

TranslationOverlay::~TranslationOverlay() = default;

void TranslationOverlay::showTranslation(const LayoutResult& layout,
                                          const QString& text,
                                          const StyleConfig& style) {
    m_text     = text;
    m_style    = style;
    m_wordWrap = layout.wordWrap;

    QFont font = style.font;
    font.setPointSize(layout.fontSize);
    m_style.font = font;

    int w = layout.bubbleWidth;
    int h = layout.bubbleHeight;

    if (m_wordWrap && w > 40) {
        // Auto-height based on wrapped text
        QFontMetrics fm(font);
        QRect br = fm.boundingRect(QRect(0, 0, w - 16, 5000),
                                   Qt::AlignLeft | Qt::TextWordWrap, text);
        h = br.height() + 20;
    }

    setGeometry(layout.position.x(), layout.position.y(), w, h);
    show();
}

void TranslationOverlay::updateStyle(const StyleConfig& style) {
    m_style = style;
    update();
}

void TranslationOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = m_style.backgroundColor;
    bg.setAlpha(static_cast<int>(m_style.backgroundAlpha * 255 / 100));

    if (m_wordWrap) {
        // Paragraph mode: slightly more opaque, word wrap, left-top aligned
        bg.setAlpha(qMin(255, bg.alpha() + 40));
        p.setBrush(bg);
        p.setPen(m_style.borderWidth > 0
            ? QPen(m_style.borderColor, m_style.borderWidth) : Qt::NoPen);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1),
                          m_style.borderRadius, m_style.borderRadius);
        p.setPen(m_style.textColor);
        p.setFont(m_style.font);
        p.drawText(rect().adjusted(10, 6, -10, -6),
                   Qt::AlignLeft | Qt::TextWordWrap, m_text);
    } else {
        // UI mode: single line, vertically centered
        p.setBrush(bg);
        p.setPen(m_style.borderWidth > 0
            ? QPen(m_style.borderColor, m_style.borderWidth) : Qt::NoPen);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1),
                          m_style.borderRadius, m_style.borderRadius);
        p.setPen(m_style.textColor);
        p.setFont(m_style.font);
        p.drawText(rect().adjusted(8, 3, -8, -3),
                   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, m_text);
    }

    // Brief green overlay for copy feedback
    if (m_flashGreen) {
        p.setBrush(QColor(0, 200, 100, 120));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1),
                          m_style.borderRadius, m_style.borderRadius);
    }
}

void TranslationOverlay::setInteractive(bool on) {
    if (m_interactive == on) return;
    m_interactive = on;
    setAttribute(Qt::WA_TransparentForMouseEvents, !on);
    setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (on) {
        exStyle &= ~WS_EX_TRANSPARENT;
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    // On "off" we don't restore WS_EX_TRANSPARENT because it's set in constructor
    update();
}

void TranslationOverlay::mousePressEvent(QMouseEvent* ev) {
    if (!m_interactive || ev->button() != Qt::LeftButton) return;
    QApplication::clipboard()->setText(m_text);
    m_flashGreen = true;
    update();
    QTimer::singleShot(300, this, [this]() {
        m_flashGreen = false;
        update();
    });
}
