#include "AreaSelector.h"
#include <QtWidgets/QApplication>
#include <QtGui/QScreen>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainterPath>
#define NOMINMAX
#include <Windows.h>

AreaSelector::AreaSelector(int screenIndex, QWidget* parent)
    : QWidget(parent), m_screenIndex(screenIndex) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setCursor(Qt::CrossCursor);

    QScreen* screen = QApplication::screens().value(screenIndex);
    if (screen) setGeometry(screen->geometry());
    showFullScreen();
    setFocus();
    activateWindow();
    grabMouse();

    // Native event filter catches Escape even when Qt focus fails
    QApplication::instance()->installNativeEventFilter(this);
}

void AreaSelector::setInitialRect(const QRect& r) {
    m_selection = r;
    m_confirmed = true;   // show handles immediately for re-edit
    m_dragging  = false;
    update();
}

AreaSelector::~AreaSelector() {
    QApplication::instance()->removeNativeEventFilter(this);
}

QRect AreaSelector::selectionRect() const {
    return QRect(
        std::min(m_dragStart.x(), m_dragEnd.x()),
        std::min(m_dragStart.y(), m_dragEnd.y()),
        std::abs(m_dragEnd.x() - m_dragStart.x()),
        std::abs(m_dragEnd.y() - m_dragStart.y())
    );
}

void AreaSelector::doCancel() {
    releaseMouse();
    QApplication::instance()->removeNativeEventFilter(this);
    emit cancelled();
    close();
}

bool AreaSelector::nativeEventFilter(const QByteArray&, void* message, qintptr*) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_KEYDOWN && msg->wParam == VK_ESCAPE) {
        doCancel();
        return true;
    }
    return false;
}

void AreaSelector::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);  // disable AA for better drag perf

    // Fill entire screen with semi-transparent dim
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.fillRect(rect(), QColor(0, 0, 0, 128));

    if (m_dragging || m_confirmed) {
        QRect sel = m_confirmed ? m_selection : selectionRect();

        // Clear the selection rectangle (punch hole) — simpler & faster than QPainterPath
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillRect(sel, Qt::black);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        // Border
        p.setPen(QPen(QColor(0, 120, 255), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(sel);

        // Handles
        p.setBrush(QColor(0, 120, 255));
        p.setPen(Qt::white);
        QVector<QPoint> handles = {
            sel.topLeft(), sel.topRight(), sel.bottomRight(), sel.bottomLeft(),
            QPoint(sel.center().x(), sel.top()),
            QPoint(sel.right(), sel.center().y()),
            QPoint(sel.center().x(), sel.bottom()),
            QPoint(sel.left(), sel.center().y()),
        };
        for (const auto& hp : handles) {
            p.drawRect(QRect(hp.x() - kHandleSize/2, hp.y() - kHandleSize/2,
                             kHandleSize, kHandleSize));
        }

        // Size info
        QString info = QString("%1 x %2").arg(sel.width()).arg(sel.height());
        p.setPen(Qt::white);
        p.drawText(sel.adjusted(4, 4, -4, -4), Qt::AlignLeft | Qt::AlignTop, info);
    }
}

int AreaSelector::handleAtPoint(const QPoint& pos) const {
    QRect sel = m_confirmed ? m_selection : selectionRect();
    QVector<QPoint> handles = {
        sel.topLeft(), sel.topRight(), sel.bottomRight(), sel.bottomLeft(),
        QPoint(sel.center().x(), sel.top()),
        QPoint(sel.right(), sel.center().y()),
        QPoint(sel.center().x(), sel.bottom()),
        QPoint(sel.left(), sel.center().y()),
    };
    for (int i = 0; i < handles.size(); ++i) {
        QRect hr(handles[i].x() - kHandleSize, handles[i].y() - kHandleSize,
                 kHandleSize * 2, kHandleSize * 2);
        if (hr.contains(pos)) return i;
    }
    if (sel.contains(pos)) return -2;
    return -1;
}

void AreaSelector::mousePressEvent(QMouseEvent* event) {
    if (m_confirmed) {
        int h = handleAtPoint(event->pos());
        if (h == -1) {
            m_confirmed = false;
            m_dragging  = true;
            m_dragStart = event->pos();
            m_dragEnd   = event->pos();
        } else if (h == -2) {
            m_activeHandle = -2;
            m_lastMousePos = event->pos();
        } else {
            m_activeHandle = h;
            m_lastMousePos = event->pos();
        }
        update();
        return;
    }

    m_dragging  = true;
    m_dragStart = event->pos();
    m_dragEnd   = event->pos();
    update();
}

void AreaSelector::mouseMoveEvent(QMouseEvent* event) {
    if (m_activeHandle == -2) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_selection.translate(delta);
        m_lastMousePos = event->pos();
        m_confirmed = true;
    } else if (m_activeHandle >= 0 && m_activeHandle < 4) {
        switch (m_activeHandle) {
        case 0: m_selection.setTopLeft(event->pos()); break;
        case 1: m_selection.setTopRight(event->pos()); break;
        case 2: m_selection.setBottomRight(event->pos()); break;
        case 3: m_selection.setBottomLeft(event->pos()); break;
        }
        m_selection = m_selection.normalized();
        m_confirmed = true;
    } else if (m_activeHandle >= 4) {
        switch (m_activeHandle) {
        case 4: m_selection.setTop(event->pos().y()); break;
        case 5: m_selection.setRight(event->pos().x()); break;
        case 6: m_selection.setBottom(event->pos().y()); break;
        case 7: m_selection.setLeft(event->pos().x()); break;
        }
        m_selection = m_selection.normalized();
        m_confirmed = true;
    } else if (m_dragging) {
        m_dragEnd = event->pos();
    }

    update();
}

void AreaSelector::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    if (m_activeHandle >= -1) {
        m_activeHandle = -1;
    }
    if (m_dragging) {
        m_dragging  = false;
        m_selection = selectionRect();
        m_confirmed = true;
        releaseMouse();
    }
}

void AreaSelector::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (!m_selection.isNull() && m_selection.width() > 10
            && m_selection.height() > 10) {
            releaseMouse();
            QApplication::instance()->removeNativeEventFilter(this);
            emit areaConfirmed(m_selection, m_screenIndex);
            close();
        }
    }
    QWidget::keyPressEvent(event);
}
