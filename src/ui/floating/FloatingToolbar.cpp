#include "FloatingToolbar.h"
#include "common/LanguageManager.h"
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
    exStyle |= WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    // Force topmost — some fullscreen apps steal Z-order even with WS_EX_TOPMOST
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

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
            "  background: rgba(255,255,255,0.07); color: #b8bfcd; border: none;"
            "  border-radius: %1px; font-size: 14px; font-weight: bold;"
            "}"
            "QPushButton:hover { background: rgba(129,140,248,0.25); color: #e0e4f0; }"
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
        "  background: rgba(99,102,241,0.35); color: #dde0e8; border: none;"
        "  border-radius: %1px; font-size: 11px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: rgba(129,140,248,0.55); }"
    ).arg(kBtnSize / 2));

    m_triggerBtn = makeBtn(QString::fromUtf8("\xe2\x96\xb6"), tr("Translate Now"));
    m_areaBtn     = makeBtn(QString::fromUtf8("\xe2\x97\xa7"), tr("Select Area"));
    m_stopBtn     = makeBtn(QString::fromUtf8("\xe2\x8f\xb9"), tr("Stop & Clear"));
    m_eyeBtn     = makeBtn(QString::fromUtf8("\xe2\x97\x89"), tr("Hide All Translations"));
    m_settingsBtn= makeBtn(QString::fromUtf8("\xe2\x9a\x99"), tr("Settings"));
    m_selTransBtn= makeBtn(QString::fromUtf8("\xe2\x9c\x82"), tr("Selection Translate"));
    m_dragBtn    = makeBtn(QString::fromUtf8("\xf0\x9f\xa4\x9a"), tr("Drag Bubbles"));

    btnLayout->addWidget(m_toggleBtn);
    btnLayout->addWidget(m_triggerBtn);
    btnLayout->addWidget(m_areaBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addWidget(m_eyeBtn);
    btnLayout->addWidget(m_selTransBtn);
    btnLayout->addWidget(m_dragBtn);
    btnLayout->addWidget(m_settingsBtn);
    m_btnContainer->setLayout(btnLayout);

    // Start collapsed
    m_triggerBtn->hide();
    m_areaBtn->hide();
    m_stopBtn->hide();
    m_eyeBtn->hide();
    m_selTransBtn->hide();
    m_dragBtn->hide();
    m_settingsBtn->hide();

    connect(m_triggerBtn, &QPushButton::clicked,
            this, &FloatingToolbar::triggerActionRequested);
    connect(m_areaBtn, &QPushButton::clicked,
            this, &FloatingToolbar::areaSelectRequested);
    connect(m_stopBtn, &QPushButton::clicked,
            this, &FloatingToolbar::stopRequested);
    connect(m_eyeBtn, &QPushButton::clicked,
            this, &FloatingToolbar::visibilityToggleRequested);
    connect(m_settingsBtn, &QPushButton::clicked,
            this, &FloatingToolbar::settingsToggleRequested);
    connect(m_selTransBtn, &QPushButton::clicked,
            this, &FloatingToolbar::selTranslateRequested);
    connect(m_dragBtn, &QPushButton::clicked,
            this, &FloatingToolbar::dragModeToggleRequested);

    m_toggleBtn->installEventFilter(this);

    m_hoverDelay.setSingleShot(true);
    m_hoverDelay.setInterval(250);
    connect(&m_hoverDelay, &QTimer::timeout, this, &FloatingToolbar::expand);

    setMouseTracking(true);

    m_anim = new QPropertyAnimation(this, "expandWidth", this);
    m_anim->setDuration(180);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    fitToScreen();

    // Periodic topmost assertion — some fullscreen apps steal Z-order
    auto* topmostTimer = new QTimer(this);
    topmostTimer->setInterval(3000);
    connect(topmostTimer, &QTimer::timeout, this, [this]() {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    });
    topmostTimer->start();

    connect(LanguageManager::instance(), &LanguageManager::languageChanged,
            this, &FloatingToolbar::retranslateUi);
}

void FloatingToolbar::setMode(Mode mode) {
    m_currentMode = mode;
    updateTriggerButton();
}

void FloatingToolbar::setGlobalVisible(bool visible) {
    m_visible = visible;
    updateEyeButton();
}

void FloatingToolbar::setSettingsOpen(bool open) {
    m_settingsOpen = open;
    updateSettingsButton();
}

void FloatingToolbar::setHasArea(bool hasArea) {
    m_hasArea = hasArea;
    updateAreaButton();
}

void FloatingToolbar::setPipelineStatus(const QString& status) {
    QString text;
    QString bgColor;
    bool pulse = false;

    if (status == QStringLiteral("idle")) {
        text = "SL";          bgColor = "rgba(99,102,241,0.35)";
    } else if (status == QStringLiteral("recognizing")) {
        text = QString::fromUtf8("\xe2\x9f\xb3");
        bgColor = "rgba(59,130,246,0.45)";           // blue
    } else if (status == QStringLiteral("translating")) {
        text = QString::fromUtf8("\xe2\x9f\xb3");
        bgColor = "rgba(245,158,11,0.45)";            // amber
        pulse = true;
    } else if (status == QStringLiteral("done")) {
        text = QString::fromUtf8("\xe2\x9c\x93");
        bgColor = "rgba(16,185,129,0.55)";            // mint
    }

    m_toggleBtn->setText(text);
    m_toggleBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none;"
        "  border-radius: %2px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(0,180,120,220); }"
    ).arg(bgColor).arg(kBtnSize / 2));

    // Pulse glow during translation — instant start, 400ms interval
    if (pulse && !m_pulseTimer) {
        // Show bright state immediately (don't wait for first timer tick)
        m_toggleBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(255,170,20,0.75); color: white;"
            "  border: 2px solid rgba(255,220,80,200); border-radius: %1px;"
            "  font-size: 11px; font-weight: bold; }"
        ).arg(kBtnSize / 2));

        m_pulseTimer = new QTimer(this);
        m_pulseTimer->setInterval(450);
        connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
            static bool dim = false; dim = !dim;
            m_toggleBtn->setStyleSheet(dim
                ? QString("QPushButton { background: rgba(180,100,0,0.50); color: #ccc;"
                          "  border: none; border-radius: %1px;"
                          "  font-size: 11px; font-weight: bold; }").arg(kBtnSize / 2)
                : QString("QPushButton { background: rgba(255,170,20,0.75); color: white;"
                          "  border: 2px solid rgba(255,220,80,200); border-radius: %1px;"
                          "  font-size: 11px; font-weight: bold; }").arg(kBtnSize / 2));
        });
        m_pulseTimer->start();
    } else if (!pulse && m_pulseTimer) {
        m_pulseTimer->stop();
        m_pulseTimer->deleteLater();
        m_pulseTimer = nullptr;
    }

    // Keep trigger button in sync: active highlight during OCR/translation
    bool busy = (status == QStringLiteral("recognizing")
              || status == QStringLiteral("translating"));
    if (busy && !m_busy) { m_busy = true;  updateTriggerButton(); }
    if (!busy && m_busy) { m_busy = false; updateTriggerButton(); }
}

void FloatingToolbar::updateTriggerButton() {
    if (m_currentMode == Mode::RealTime) {
        m_triggerBtn->setText(QString::fromUtf8("\xe2\x8f\xb8"));  // pause
        m_triggerBtn->setToolTip(tr("Pause"));
        m_triggerBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(255,255,255,0.07); color: #b8bfcd; border: none;"
            "  border-radius: %1px; font-size: 14px; font-weight: bold; }"
            "QPushButton:hover { background: rgba(129,140,248,0.25); color: #e0e4f0; }"
        ).arg(kBtnSize / 2));
    } else if (m_currentMode == Mode::Pause) {
        m_triggerBtn->setText(QString::fromUtf8("\xe2\x96\xb6"));  // play
        m_triggerBtn->setToolTip(tr("Resume"));
        m_triggerBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(255,255,255,0.07); color: #b8bfcd; border: none;"
            "  border-radius: %1px; font-size: 14px; font-weight: bold; }"
            "QPushButton:hover { background: rgba(129,140,248,0.25); color: #e0e4f0; }"
        ).arg(kBtnSize / 2));
    } else if (m_busy) {
        // Snapshot mode + translating: show active/highlighted
        m_triggerBtn->setText(QString::fromUtf8("\xe2\x9f\xb3"));  // spinner
        m_triggerBtn->setToolTip(tr("Translating..."));
        m_triggerBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(245,158,11,0.45); color: white; border: none;"
            "  border-radius: %1px; font-size: 14px; font-weight: bold; }"
        ).arg(kBtnSize / 2));
    } else {
        m_triggerBtn->setText(QString::fromUtf8("\xe2\x96\xb6"));  // play
        m_triggerBtn->setToolTip(tr("Translate Now"));
        m_triggerBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(255,255,255,0.07); color: #b8bfcd; border: none;"
            "  border-radius: %1px; font-size: 14px; font-weight: bold; }"
            "QPushButton:hover { background: rgba(129,140,248,0.25); color: #e0e4f0; }"
        ).arg(kBtnSize / 2));
    }
}

void FloatingToolbar::updateAreaButton() {
    if (m_hasArea) {
        m_areaBtn->setText(QString::fromUtf8("\xe2\x96\xa3"));
        m_areaBtn->setToolTip(tr("Adjust Area"));
        m_areaBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(59,130,246,0.40); color: #dde0e8; border: none;"
            "  border-radius: %1px; font-size: 14px; font-weight: bold; }"
            "QPushButton:hover { background: rgba(59,130,246,0.60); }"
        ).arg(kBtnSize / 2));
    } else {
        m_areaBtn->setText(QString::fromUtf8("\xe2\x97\xa7"));
        m_areaBtn->setToolTip(tr("Select Area"));
        m_areaBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(255,255,255,0.07); color: #b8bfcd; border: none;"
            "  border-radius: %1px; font-size: 14px; font-weight: bold; }"
            "QPushButton:hover { background: rgba(129,140,248,0.25); color: #e0e4f0; }"
        ).arg(kBtnSize / 2));
    }
}

void FloatingToolbar::updateEyeButton() {
    if (m_visible) {
        m_eyeBtn->setText(QString::fromUtf8("\xe2\x97\x89"));
        m_eyeBtn->setToolTip(tr("Hide All Translations"));
        m_eyeBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(16,185,129,0.30); color: #dde0e8; border: none;"
            "  border-radius: %1px; font-size: 14px; }"
            "QPushButton:hover { background: rgba(16,185,129,0.50); }"
        ).arg(kBtnSize / 2));
    } else {
        m_eyeBtn->setText(QString::fromUtf8("\xe2\x97\x8c"));
        m_eyeBtn->setToolTip(tr("Show All Translations"));
        m_eyeBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(245,158,11,0.25); color: #dde0e8; border: none;"
            "  border-radius: %1px; font-size: 14px; }"
            "QPushButton:hover { background: rgba(245,158,11,0.45); }"
        ).arg(kBtnSize / 2));
    }
}

void FloatingToolbar::updateSettingsButton() {
    if (m_settingsOpen) {
        m_settingsBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(99,102,241,0.40); color: #dde0e8; border: none;"
            "  border-radius: %1px; font-size: 14px; }"
            "QPushButton:hover { background: rgba(239,68,68,0.50); }"
        ).arg(kBtnSize / 2));
    } else {
        m_settingsBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(255,255,255,0.07); color: #b8bfcd; border: none;"
            "  border-radius: %1px; font-size: 14px; }"
            "QPushButton:hover { background: rgba(129,140,248,0.25); color: #e0e4f0; }"
        ).arg(kBtnSize / 2));
    }
}

void FloatingToolbar::setSelectionMode(bool on) {
    m_selModeActive = on;
    updateSelTransButton();
}

void FloatingToolbar::updateSelTransButton() {
    if (m_selModeActive) {
        m_selTransBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(16,185,129,0.35); color: #dde0e8; border: none;"
            "  border-radius: %1px; font-size: 14px; }"
            "QPushButton:hover { background: rgba(16,185,129,0.55); }"
        ).arg(kBtnSize / 2));
    } else {
        m_selTransBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(255,255,255,0.07); color: #b8bfcd; border: none;"
            "  border-radius: %1px; font-size: 14px; }"
            "QPushButton:hover { background: rgba(129,140,248,0.25); color: #e0e4f0; }"
        ).arg(kBtnSize / 2));
    }
}

void FloatingToolbar::setDragMode(bool on) {
    m_dragModeActive = on;
    updateDragButton();
}

void FloatingToolbar::updateDragButton() {
    if (m_dragModeActive) {
        m_dragBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(16,185,129,0.35); color: #dde0e8; border: none;"
            "  border-radius: %1px; font-size: 14px; }"
            "QPushButton:hover { background: rgba(16,185,129,0.55); }"
        ).arg(kBtnSize / 2));
    } else {
        m_dragBtn->setStyleSheet(QString(
            "QPushButton { background: rgba(255,255,255,0.07); color: #b8bfcd; border: none;"
            "  border-radius: %1px; font-size: 14px; }"
            "QPushButton:hover { background: rgba(129,140,248,0.25); color: #e0e4f0; }"
        ).arg(kBtnSize / 2));
    }
}

void FloatingToolbar::retranslateUi() {
    m_toggleBtn->setToolTip(tr("ScreenLingo — drag to move"));
    m_stopBtn->setToolTip(tr("Stop & Clear"));
    m_settingsBtn->setToolTip(tr("Settings"));
    m_selTransBtn->setToolTip(tr("Selection Translate"));
    m_dragBtn->setToolTip(tr("Drag Bubbles"));
    updateTriggerButton();
    updateAreaButton();
    updateEyeButton();
    updateSelTransButton();
    updateDragButton();
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
    int delta = m_expandWidth - w;
    m_expandWidth = w;
    setFixedWidth(w);
    m_btnContainer->setGeometry(0, 0, w, height());
    if (delta > 0) move(x() + delta, y());
    else if (delta < 0) move(x() + delta, y());
}

void FloatingToolbar::expand() {
    if (m_expanded) return;
    m_expanded = true;
    m_triggerBtn->show();
    m_areaBtn->show();
    m_stopBtn->show();
    m_eyeBtn->show();
    m_selTransBtn->show();
    m_dragBtn->show();
    m_settingsBtn->show();
    updateTriggerButton();
    updateAreaButton();
    updateEyeButton();
    updateSettingsButton();
    m_anim->stop();
    m_anim->setStartValue(m_expandWidth);
    m_anim->setEndValue(m_fullWidth);
    m_anim->start();
}

void FloatingToolbar::collapse() {
    if (!m_expanded) return;
    m_expanded = false;
    m_triggerBtn->hide();
    m_areaBtn->hide();
    m_stopBtn->hide();
    m_eyeBtn->hide();
    m_selTransBtn->hide();
    m_dragBtn->hide();
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

bool FloatingToolbar::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_toggleBtn) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragStartPos = me->globalPosition().toPoint();
                m_dragStartGeo = frameGeometry().topLeft();
                m_hoverDelay.stop();
                m_toggleBtn->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            if (m_dragging) {
                auto* me = static_cast<QMouseEvent*>(event);
                QPoint delta = me->globalPosition().toPoint() - m_dragStartPos;
                move(m_dragStartGeo + delta);
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_dragging) {
                m_dragging = false;
                m_toggleBtn->setCursor(Qt::OpenHandCursor);
                auto* me = static_cast<QMouseEvent*>(event);
                int dist = (me->globalPosition().toPoint() - m_dragStartPos).manhattanLength();
                if (dist < 5) {
                    if (m_expanded) collapse(); else expand();
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void FloatingToolbar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_expanded) {
        // Glass background with subtle border
        p.setBrush(QColor(24, 26, 32, 220));
        p.setPen(QPen(QColor(255, 255, 255, 20), 1));
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 20, 20);
    }
}
