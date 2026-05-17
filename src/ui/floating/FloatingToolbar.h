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
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void expand();
    void collapse();
    int  expandWidth() const { return m_expandWidth; }
    void setExpandWidth(int w);
    void reposition();

    QWidget*             m_btnContainer = nullptr;
    QPushButton*         m_toggleBtn    = nullptr;
    QPushButton*         m_playBtn      = nullptr;
    QPushButton*         m_pauseBtn     = nullptr;
    QPushButton*         m_areaBtn      = nullptr;
    QPushButton*         m_eyeBtn       = nullptr;
    QPushButton*         m_settingsBtn  = nullptr;

    QPropertyAnimation*  m_anim = nullptr;
    int  m_expandWidth  = 44;   // collapsed size
    int  m_fullWidth    = 240;  // expanded size
    bool m_expanded     = false;
    bool m_paused       = false;

    QTimer m_hoverDelay;
    static constexpr int kBtnSize = 36;
    static constexpr int kGap     = 4;
};
