#include "SubtitleOverlay.h"
#include <QtWidgets/QApplication>
#include <QtGui/QScreen>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>

#include <windows.h>

SubtitleOverlay::SubtitleOverlay(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
                   Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        m_screenWidth  = screen->geometry().width();
        m_screenHeight = screen->geometry().height();
    }
    hide();
}

void SubtitleOverlay::showText(const QString& text, const StyleConfig& style) {
    m_text  = text;
    m_style = style;

    if (text.isEmpty()) { hide(); return; }

    // Calculate size based on text content
    int fontSize = style.font.pointSize();
    QFont font(style.font);
    font.setPointSize(qMax(fontSize, 18));  // minimum readable size for subtitles
    QFontMetrics fm(font);

    int maxWidth  = static_cast<int>(m_screenWidth * 0.9);
    int sideMargin = static_cast<int>(m_screenWidth * 0.05);

    // Measure text with word wrap
    QRect textRect = fm.boundingRect(QRect(0, 0, maxWidth - 40, 0),
                                      Qt::AlignCenter | Qt::TextWordWrap, text);
    int contentW = qMax(textRect.width() + 40, 200);
    int contentH = textRect.height() + 24;

    // Place at TOP of screen to avoid overlapping the OCR capture area
    // (video subtitles are at the bottom, so we put translation at the top)
    int x = sideMargin + (maxWidth - contentW) / 2;
    int y = 20;  // small top margin

    setGeometry(x, y, contentW, contentH);
    show();
    raise();
}

void SubtitleOverlay::hide() {
    QWidget::hide();
}

void SubtitleOverlay::clear() {
    m_text.clear();
    hide();
}

void SubtitleOverlay::paintEvent(QPaintEvent*) {
    if (m_text.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Semi-transparent black background with rounded corners
    QColor bg(0, 0, 0, 165);  // ~65% opacity
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 12, 12);

    // White text, centered
    int fontSize = qMax(m_style.font.pointSize(), 18);
    QFont font(m_style.font);
    font.setPointSize(fontSize);
    p.setFont(font);
    p.setPen(Qt::white);

    QRect textRect = rect().adjusted(20, 12, -20, -12);
    p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, m_text);
}
