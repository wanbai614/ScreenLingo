#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include "common/Types.h"

class FloatingToolbar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int expandWidth READ expandWidth WRITE setExpandWidth)

public:
    explicit FloatingToolbar(QWidget* parent = nullptr);

    void setPaused(bool paused);

signals:
    void startRequested();
    void pauseRequested();
    void areaSelectRequested();
    void visibilityToggleRequested();
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    void expand();
    void collapse();
    int  expandWidth() const { return m_expandWidth; }
    void setExpandWidth(int w);
    void fitToScreen();

    QWidget*             m_btnContainer  = nullptr;
    QPushButton*         m_toggleBtn     = nullptr;
    QPushButton*         m_playBtn       = nullptr;
    QPushButton*         m_pauseBtn      = nullptr;
    QPushButton*         m_areaBtn       = nullptr;
    QPushButton*         m_eyeBtn        = nullptr;
    QPushButton*         m_settingsBtn   = nullptr;

    QPropertyAnimation*  m_anim = nullptr;
    int  m_expandWidth  = 44;
    int  m_fullWidth    = 240;
    bool m_expanded     = false;
    bool m_paused       = false;
    bool m_dragging     = false;
    QPoint m_dragOffset;

    QTimer m_hoverDelay;
    static constexpr int kBtnSize  = 36;
    static constexpr int kGap      = 4;
    static constexpr int kMargin   = 4;
    static constexpr int kCollapsedW = 44;
};
