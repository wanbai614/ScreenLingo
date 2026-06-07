#include "TranslationPopup.h"
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtGui/QScreen>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>

static constexpr int kMaxPopupW     = 420;
static constexpr int kMinPopupW     = 180;
static constexpr int kPadH          = 14;
static constexpr int kPadW          = 16;
static constexpr int kLineGap       = 8;
static constexpr int kTopBarH       = 22;  // space for close button

TranslationPopup::TranslationPopup(QWidget* parent)
    : QWidget(parent) {

    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
                   Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
}

TranslationPopup::~TranslationPopup() = default;

QRect TranslationPopup::closeBtnRect() const {
    return QRect(width() - kCloseBtnSize - 6, 4,
                 kCloseBtnSize, kCloseBtnSize);
}

void TranslationPopup::showTranslation(const QPoint& screenPos,
                                        const QString& original,
                                        const QString& translated) {
    // Destroy previous bubble
    hide();

    m_original   = original;
    m_translated = translated;
    m_hoverClose = false;

    QFont origFont("Microsoft YaHei", 9);
    QFont transFont("Microsoft YaHei", 13);
    QFontMetrics origFm(origFont);
    QFontMetrics transFm(transFont);

    QScreen* screen = QApplication::screenAt(screenPos);
    if (!screen) screen = QApplication::primaryScreen();
    QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int spaceRight = avail.right() - screenPos.x() - 24;
    int spaceLeft  = screenPos.x() - avail.left() - 24;
    int maxW = qMin(kMaxPopupW, qMax(spaceRight, spaceLeft));
    maxW = qMax(maxW, kMinPopupW);

    int textAreaW = maxW - kPadW * 2;

    // Word-wrap measurements
    QRect origBounds = origFm.boundingRect(QRect(0, 0, textAreaW, 2000),
                                           Qt::AlignLeft | Qt::TextWordWrap,
                                           m_original);
    QRect transBounds = transFm.boundingRect(QRect(0, 0, textAreaW, 5000),
                                              Qt::AlignLeft | Qt::TextWordWrap,
                                              m_translated);

    int contentW = textAreaW;
    int popupW   = contentW + kPadW * 2;
    int popupH   = kTopBarH + origBounds.height() + transBounds.height()
                   + kPadH + kLineGap;

    // Shrink if text is short
    int singleOrigW  = origFm.boundingRect(m_original).width();
    int singleTransW = transFm.boundingRect(m_translated).width();
    int naturalW = qMax(singleOrigW, singleTransW) + kPadW * 2;
    if (naturalW < popupW) popupW = qMax(naturalW, kMinPopupW);
    popupW = qMin(popupW, maxW);

    setFixedSize(popupW, popupH);

    // Position to the RIGHT of cursor
    int x = screenPos.x() + 16;
    int y = screenPos.y() - popupH / 2;

    if (x + popupW > avail.right())
        x = screenPos.x() - popupW - 16;
    if (x < avail.left())  x = avail.left() + 4;
    if (x + popupW > avail.right()) x = avail.right() - popupW - 4;
    if (y < avail.top())  y = avail.top() + 4;
    if (y + popupH > avail.bottom()) y = avail.bottom() - popupH - 4;

    move(x, y);
    show();
    raise();
}

void TranslationPopup::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.setBrush(QColor(28, 30, 35, 238));
    p.setPen(QPen(QColor(80, 80, 90, 160), 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 10, 10);

    int textW = width() - kPadW * 2;
    int topY  = kTopBarH;

    // --- Close button (✕) ---
    QRect cb = closeBtnRect();
    p.setPen(Qt::NoPen);
    p.setBrush(m_hoverClose ? QColor(200, 60, 60, 200)
                            : QColor(120, 120, 130, 120));
    p.drawRoundedRect(cb, 4, 4);

    p.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    p.setPen(QColor(220, 220, 225));
    p.drawText(cb, Qt::AlignCenter, QStringLiteral("✕"));

    // --- Original text ---
    p.setFont(QFont("Microsoft YaHei", 9));
    p.setPen(QColor(170, 170, 180));
    QFontMetrics origFm = p.fontMetrics();
    QRect ob = origFm.boundingRect(QRect(0, 0, textW, 2000),
                                   Qt::AlignLeft | Qt::TextWordWrap,
                                   m_original);
    QRect origRect(kPadW, topY, textW, ob.height());
    p.drawText(origRect, Qt::AlignLeft | Qt::TextWordWrap, m_original);
    topY += ob.height() + kLineGap;

    // --- Translated text ---
    p.setFont(QFont("Microsoft YaHei", 13));
    p.setPen(QColor(240, 240, 245));
    QFontMetrics transFm = p.fontMetrics();
    QRect tb = transFm.boundingRect(QRect(0, 0, textW, 5000),
                                    Qt::AlignLeft | Qt::TextWordWrap,
                                    m_translated);
    QRect transRect(kPadW, topY, textW, tb.height());
    p.drawText(transRect, Qt::AlignLeft | Qt::TextWordWrap, m_translated);
}

void TranslationPopup::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    if (closeBtnRect().contains(event->pos())) {
        close();
        return;
    }

    // Start dragging
    m_dragging   = true;
    m_dragOffset = event->pos();
    setCursor(Qt::ClosedHandCursor);
}

void TranslationPopup::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        QPoint global = event->globalPosition().toPoint();
        move(global - m_dragOffset);
        return;
    }

    bool inBtn = closeBtnRect().contains(event->pos());
    setCursor(inBtn ? Qt::ArrowCursor : Qt::OpenHandCursor);
    if (inBtn != m_hoverClose) {
        m_hoverClose = inBtn;
        update();
    }
}

void TranslationPopup::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
}
