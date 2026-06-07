#include "AreaOverlay.h"
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>
#define NOMINMAX
#include <windows.h>

AreaOverlay::AreaOverlay(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
                   Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
}

AreaOverlay::~AreaOverlay() = default;

void AreaOverlay::setArea(const QRect& screenRect) {
    m_area = screenRect;
    // Position the overlay window to cover the area with padding for handles
    const int pad = kHandleSize + kBorderWidth + 4;
    setGeometry(screenRect.adjusted(-pad, -pad, pad, pad));
    show();
}

QRect AreaOverlay::normalizedArea() const {
    // Convert m_area from window-local coords back to screen coords
    return QRect(
        kHandleSize + kBorderWidth + 4,
        kHandleSize + kBorderWidth + 4,
        m_area.width(), m_area.height()
    );
}

void AreaOverlay::setEditing(bool editing) {
    if (m_editing == editing) return;
    m_editing = editing;

    setAttribute(Qt::WA_TransparentForMouseEvents, !editing);
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (editing) {
        exStyle &= ~WS_EX_TRANSPARENT;
        setFocus();
    } else {
        exStyle |= WS_EX_TRANSPARENT;
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    update();
}

void AreaOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect inner = normalizedArea();

    // Semi-transparent dim outside the area
    p.setBrush(QColor(0, 0, 0, m_editing ? 80 : 20));
    p.setPen(Qt::NoPen);
    QPainterPath full;
    full.addRect(rect());
    QPainterPath hole;
    hole.addRect(inner.adjusted(-1, -1, 1, 1));
    p.drawPath(full.subtracted(hole));

    // Border
    QColor borderColor = m_editing ? QColor(0, 160, 255) : QColor(0, 120, 255, 140);
    p.setPen(QPen(borderColor, kBorderWidth, Qt::SolidLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(inner);

    // Handles only in edit mode
    if (m_editing) {
        p.setBrush(QColor(0, 160, 255));
        p.setPen(Qt::white);
        QVector<QPoint> handles = {
            inner.topLeft(), inner.topRight(),
            inner.bottomRight(), inner.bottomLeft(),
            QPoint(inner.center().x(), inner.top()),
            QPoint(inner.right(), inner.center().y()),
            QPoint(inner.center().x(), inner.bottom()),
            QPoint(inner.left(), inner.center().y()),
        };
        for (const auto& hp : handles) {
            p.drawRect(QRect(hp.x() - kHandleSize/2, hp.y() - kHandleSize/2,
                             kHandleSize, kHandleSize));
        }

        // Size info
        QString info = QString("%1 x %2").arg(m_area.width()).arg(m_area.height());
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);
        p.drawText(inner.adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignTop, info);
    }
}

int AreaOverlay::handleAtPoint(const QPoint& pos) const {
    QRect inner = normalizedArea();
    QVector<QPoint> handles = {
        inner.topLeft(), inner.topRight(),
        inner.bottomRight(), inner.bottomLeft(),
        QPoint(inner.center().x(), inner.top()),
        QPoint(inner.right(), inner.center().y()),
        QPoint(inner.center().x(), inner.bottom()),
        QPoint(inner.left(), inner.center().y()),
    };
    for (int i = 0; i < handles.size(); ++i) {
        QRect hr(handles[i].x() - kHandleSize, handles[i].y() - kHandleSize,
                 kHandleSize * 2, kHandleSize * 2);
        if (hr.contains(pos)) return i;
    }
    if (inner.contains(pos)) return -2; // body drag
    return -1; // outside
}

void AreaOverlay::mousePressEvent(QMouseEvent* event) {
    if (!m_editing || event->button() != Qt::LeftButton) return;

    m_activeHandle = handleAtPoint(event->pos());
    if (m_activeHandle == -1) {
        // Clicked outside → cancel editing
        setEditing(false);
        emit editCancelled();
        return;
    }
    m_dragStartPos = event->pos();
    m_editStartArea = m_area;
}

void AreaOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (!m_editing) return;

    QPoint delta = event->pos() - m_dragStartPos;

    if (m_activeHandle == -2) {
        // Body drag → move
        m_area = m_editStartArea.translated(delta);
    } else if (m_activeHandle >= 0 && m_activeHandle < 4) {
        // Corner resize
        QRect r = m_editStartArea;
        switch (m_activeHandle) {
        case 0: r.setTopLeft(r.topLeft() + delta); break;
        case 1: r.setTopRight(r.topRight() + delta); break;
        case 2: r.setBottomRight(r.bottomRight() + delta); break;
        case 3: r.setBottomLeft(r.bottomLeft() + delta); break;
        }
        r = r.normalized();
        if (r.width() >= 20 && r.height() >= 20)
            m_area = r;
    } else if (m_activeHandle >= 4) {
        // Midpoint resize
        QRect r = m_editStartArea;
        switch (m_activeHandle) {
        case 4: r.setTop(r.top() + delta.y()); break;
        case 5: r.setRight(r.right() + delta.x()); break;
        case 6: r.setBottom(r.bottom() + delta.y()); break;
        case 7: r.setLeft(r.left() + delta.x()); break;
        }
        r = r.normalized();
        if (r.width() >= 20 && r.height() >= 20)
            m_area = r;
    }

    // Reposition overlay window based on current area
    const int pad = kHandleSize + kBorderWidth + 4;
    QPoint topLeft = m_area.topLeft();
    QSize newSize(m_area.width() + pad * 2, m_area.height() + pad * 2);
    setGeometry(QRect(topLeft.x() - pad, topLeft.y() - pad,
                      newSize.width(), newSize.height()));
    update();
}

void AreaOverlay::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    if (m_editing && m_activeHandle != -1) {
        m_activeHandle = -1;
        emit areaChanged(m_area);
    }
}

void AreaOverlay::keyPressEvent(QKeyEvent* event) {
    if (m_editing && event->key() == Qt::Key_Escape) {
        // Restore original area and exit edit mode
        m_area = m_editStartArea;
        setEditing(false);
        emit editCancelled();
    }
    if (m_editing && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        setEditing(false);
        emit areaChanged(m_area);
    }
    QWidget::keyPressEvent(event);
}
