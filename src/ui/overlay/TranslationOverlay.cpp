#include "TranslationOverlay.h"
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
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
    m_text  = text;
    m_style = style;

    QFont font = style.font;
    font.setPointSize(layout.fontSize);
    m_style.font = font;

    // Use exact dimensions computed by LayoutEngine to avoid mismatch
    setGeometry(layout.position.x(), layout.position.y(),
                layout.bubbleWidth, layout.bubbleHeight);
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
    p.setBrush(bg);
    p.setPen(QPen(m_style.borderColor, m_style.borderWidth));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1),
                      m_style.borderRadius, m_style.borderRadius);

    p.setPen(m_style.textColor);
    p.setFont(m_style.font);
    QRect textRect = rect().adjusted(12, 8, -12, -8);
    p.drawText(textRect, Qt::AlignLeft | Qt::TextWordWrap, m_text);
}
