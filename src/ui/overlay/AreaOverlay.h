#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QRect>
#include <QtCore/QPoint>

// Persistent overlay that draws a border around the selected capture area.
// In normal mode it is click-through.  In edit mode it shows resize handles
// and supports drag-to-move and handle-drag-to-resize.
class AreaOverlay : public QWidget {
    Q_OBJECT

public:
    explicit AreaOverlay(QWidget* parent = nullptr);
    ~AreaOverlay() override;

    // Set the geometry to display (screen coords)
    void setArea(const QRect& screenRect);

    // Toggle edit mode: show handles, accept mouse input
    void setEditing(bool editing);
    bool isEditing() const { return m_editing; }

signals:
    // Emitted when user finishes editing (mouse release after move/resize)
    void areaChanged(const QRect& newArea);
    // Emitted when user cancels editing (Escape)
    void editCancelled();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    // Handle types: -1=none, -2=body drag, 0-7=corner/mid handles
    int  handleAtPoint(const QPoint& pos) const;
    QRect normalizedArea() const;

    bool   m_editing     = false;
    QRect  m_area;         // display rect in this widget's coordinates
    QRect  m_editStartArea; // snapshot when editing begins
    QPoint m_dragStartPos;
    int    m_activeHandle = -1;  // handle being dragged

    static constexpr int kHandleSize = 8;
    static constexpr int kBorderWidth = 2;
};
