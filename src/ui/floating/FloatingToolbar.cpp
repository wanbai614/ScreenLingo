#include "FloatingToolbar.h"
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QMouseEvent>
#include <QtGui/QEnterEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <Windows.h>

FloatingToolbar::FloatingToolbar(QWidget* parent)
    : QWidget(parent) {

    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
                   Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_NOACTIVATE;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    setFixedHeight(kBtnSize + kMargin);
    setFixedWidth(m_expandWidth);

    m_btnContainer = new QWidget(this);
    m_btnContainer->setStyleSheet("background: transparent;");
    auto* btnLayout = new QHBoxLayout(m_btnContainer);
    btnLayout->setContentsMargins(kMargin / 2, kMargin / 2, kMargin / 2, kMargin / 2);
    btnLayout->setSpacing(kGap);

    auto makeBtn = [&](const QString& text, const QString& tooltip) {
        auto* btn = new QPushButton(text, m_btnContainer);
        btn->setFixedSize(kBtnSize, kBtnSize);
        btn->setToolTip(tooltip);
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background: rgba(60,60,60,160); color: white; border: none;"
            "  border-radius: %1px; font-size: 14px; font-weight: bold;"
            "}"
            "QPushButton:hover { background: rgba(0,140,100,200); }"
        ).arg(kBtnSize / 2));
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    m_toggleBtn = new QPushButton("SL", m_btnContainer);
    m_toggleBtn->setFixedSize(kBtnSize, kBtnSize);
    m_toggleBtn->setToolTip(tr("ScreenLingo — drag to move"));
    m_toggleBtn->setCursor(Qt::OpenHandCursor);
    m_toggleBtn->setStyleSheet(QString(
        "QPushButton {"
        "  background: rgba(0,150,100,180); color: white; border: none;"
        "  border-radius: %1px; font-size: 11px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: rgba(0,180,120,220); }"
    ).arg(kBtnSize / 2));

    m_playBtn    = makeBtn("▶", tr("Start Translation"));
    m_pauseBtn   = makeBtn("⏸", tr("Pause Translation"));
    m_areaBtn    = makeBtn("◧", tr("Select Area"));
    m_eyeBtn     = makeBtn("👁", tr("Show/Hide Translations"));
    m_settingsBtn= makeBtn("⚙", tr("Settings"));

    btnLayout->addWidget(m_toggleBtn);
    btnLayout->addWidget(m_playBtn);
    btnLayout->addWidget(m_pauseBtn);
    btnLayout->addWidget(m_areaBtn);
    btnLayout->addWidget(m_eyeBtn);
    btnLayout->addWidget(m_settingsBtn);
    m_btnContainer->setLayout(btnLayout);

    m_pauseBtn->hide();

    connect(m_toggleBtn, &QPushButton::clicked, this, [this]() {
        if (m_expanded) collapse(); else expand();
    });
    connect(m_playBtn, &QPushButton::clicked, this, &FloatingToolbar::startRequested);
    connect(m_pauseBtn, &QPushButton::clicked, this, &FloatingToolbar::pauseRequested);
    connect(m_areaBtn, &QPushButton::clicked, this, &FloatingToolbar::areaSelectRequested);
    connect(m_eyeBtn, &QPushButton::clicked, this, &FloatingToolbar::visibilityToggleRequested);
    connect(m_settingsBtn, &QPushButton::clicked, this, &FloatingToolbar::settingsRequested);

    m_hoverDelay.setSingleShot(true);
    m_hoverDelay.setInterval(250);
    connect(&m_hoverDelay, &QTimer::timeout, this, &FloatingToolbar::expand);

    setMouseTracking(true);

    m_anim = new QPropertyAnimation(this, "expandWidth", this);
    m_anim->setDuration(180);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    // Start at right edge, vertically centered
    fitToScreen();
}

void FloatingToolbar::setPaused(bool paused) {
    m_paused = paused;
    m_playBtn->setVisible(paused);
    m_pauseBtn->setVisible(!paused);
}

void FloatingToolbar::fitToScreen() {
    QScreen* screen = QApplication::primaryScreen();
    if (!screen) return;
    QRect avail = screen->availableGeometry();
    int x = avail.right() - kCollapsedW - kMargin;
    int y = (avail.top() + avail.bottom()) / 2 - height() / 2;
    move(x, y);
}

void FloatingToolbar::setExpandWidth(int w) {
    // Keep right edge anchored during animation
    int delta = m_expandWidth - w;
    m_expandWidth = w;
    setFixedWidth(w);
    m_btnContainer->setGeometry(0, 0, w, height());
    if (delta > 0) {
        move(x() + delta, y());  // expand leftward
    } else if (delta < 0) {
        move(x() + delta, y());  // shrink rightward
    }
}

void FloatingToolbar::expand() {
    if (m_expanded) return;
    m_expanded = true;
    // Show all action buttons before expanding
    m_playBtn->show();
    m_areaBtn->show();
    m_eyeBtn->show();
    m_settingsBtn->show();
    setPaused(m_paused);  // restore play/pause visibility
    m_anim->stop();
    m_anim->setStartValue(m_expandWidth);
    m_anim->setEndValue(m_fullWidth);
    m_anim->start();
}

void FloatingToolbar::collapse() {
    if (!m_expanded) return;
    m_expanded = false;
    // Hide action buttons immediately before shrinking
    m_playBtn->hide();
    m_pauseBtn->hide();
    m_areaBtn->hide();
    m_eyeBtn->hide();
    m_settingsBtn->hide();
    m_anim->stop();
    m_anim->setStartValue(m_expandWidth);
    m_anim->setEndValue(kCollapsedW);
    m_anim->start();
}

void FloatingToolbar::enterEvent(QEnterEvent*) {
    if (!m_dragging) m_hoverDelay.start();
}

void FloatingToolbar::leaveEvent(QEvent*) {
    m_hoverDelay.stop();
    if (m_expanded) {
        QTimer::singleShot(600, this, [this]() {
            if (!underMouse() && !m_dragging) collapse();
        });
    }
}

void FloatingToolbar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_hoverDelay.stop();
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void FloatingToolbar::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        QPoint newPos = event->globalPosition().toPoint() - m_dragOffset;
        move(newPos);
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingToolbar::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        if (m_dragOffset.manhattanLength() < 5) {
            // Just a click, not a drag — toggle expand
            if (m_expanded) collapse(); else expand();
        }
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void FloatingToolbar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_expanded) {
        p.setBrush(QColor(30, 30, 30, 130));
        p.setPen(QPen(QColor(80, 80, 80, 80), 1));
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 20, 20);
    } else {
        // Collapsed: small rounded background behind the SL button
        p.setBrush(QColor(0, 0, 0, 1));  // nearly invisible, just for hit-test
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(2, 2, kCollapsedW - 4, height() - 4, 18, 18);
    }
}
