#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QRect>
#include <QtCore/QAbstractNativeEventFilter>
#include <QtGui/QPainter>

class AreaSelector : public QWidget, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit AreaSelector(int screenIndex, QWidget* parent = nullptr);
    ~AreaSelector() override;
    void setInitialRect(const QRect& r);

signals:
    void areaConfirmed(const QRect& area, int screenIndex);
    void cancelled();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    QRect selectionRect() const;
    int  handleAtPoint(const QPoint& pos) const;

    void doCancel();

    int    m_screenIndex;
    QPoint m_dragStart;
    QPoint m_dragEnd;
    bool   m_dragging  = false;
    bool   m_confirmed = false;
    QRect  m_selection;

    int    m_activeHandle = -1;
    QPoint m_lastMousePos;

    static constexpr int kHandleSize = 8;
};
