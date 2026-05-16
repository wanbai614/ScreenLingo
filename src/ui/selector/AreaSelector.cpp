#include "AreaSelector.h"
#include <QtWidgets/QApplication>
#include <QtGui/QScreen>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainterPath>

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
    grabMouse();
    grabKeyboard();
}

QRect AreaSelector::selectionRect() const {
    return QRect(
        std::min(m_dragStart.x(), m_dragEnd.x()),
        std::min(m_dragStart.y(), m_dragEnd.y()),
        std::abs(m_dragEnd.x() - m_dragStart.x()),
        std::abs(m_dragEnd.y() - m_dragStart.y())
    );
}

void AreaSelector::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), QColor(0, 0, 0, 128));

    if (m_dragging || m_confirmed) {
        QRect sel = m_confirmed ? m_selection : selectionRect();

        QPainterPath path;
        path.addRect(rect());
        path.addRect(sel);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillPath(path, Qt::black);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        p.setPen(QPen(QColor(0, 120, 255), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(sel);

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
    if (sel.contains(pos)) return -2;  // inside selection -> move
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
        releaseKeyboard();
    }
}

void AreaSelector::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!m_selection.isNull() && m_selection.width() > 10
            && m_selection.height() > 10) {
            releaseMouse();
            releaseKeyboard();
            emit areaConfirmed(m_selection, m_screenIndex);
            close();
        }
        break;
    case Qt::Key_Escape:
        releaseMouse();
        releaseKeyboard();
        emit cancelled();
        close();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}
