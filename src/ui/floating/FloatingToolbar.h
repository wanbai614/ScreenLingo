#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QTimer>
#include <QtCore/QPoint>

class FloatingToolbar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int expandWidth READ expandWidth WRITE setExpandWidth)

public:
    explicit FloatingToolbar(QWidget* parent = nullptr);

    void setPaused(bool paused);
    void setGlobalVisible(bool visible);
    void setSettingsOpen(bool open);

signals:
    void startRequested();
    void pauseRequested();
    void areaSelectRequested();
    void visibilityToggleRequested();
    void settingsToggleRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void expand();
    void collapse();
    void updateEyeButton();
    void updateSettingsButton();
    int  expandWidth() const { return m_expandWidth; }
    void setExpandWidth(int w);
    void fitToScreen();

    QPushButton* m_toggleBtn    = nullptr;
    QPushButton* m_playBtn      = nullptr;
    QPushButton* m_pauseBtn     = nullptr;
    QPushButton* m_areaBtn      = nullptr;
    QPushButton* m_eyeBtn       = nullptr;
    QPushButton* m_settingsBtn  = nullptr;
    QWidget*     m_btnContainer = nullptr;

    QPropertyAnimation* m_anim = nullptr;
    int  m_expandWidth   = 44;
    int  m_fullWidth     = 240;
    bool m_expanded      = false;
    bool m_paused        = false;
    bool m_visible       = true;
    bool m_settingsOpen  = false;
    bool m_dragging      = false;
    QPoint m_dragStartPos;
    QPoint m_dragStartGeo;

    QTimer m_hoverDelay;
    static constexpr int kBtnSize    = 36;
    static constexpr int kGap        = 4;
    static constexpr int kMargin     = 4;
    static constexpr int kCollapsedW = 44;
};
