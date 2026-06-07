#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QTimer>
#include <QtCore/QPoint>
#include "common/Types.h"

class FloatingToolbar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int expandWidth READ expandWidth WRITE setExpandWidth)

public:
    explicit FloatingToolbar(QWidget* parent = nullptr);

    void setMode(Mode mode);
    void setGlobalVisible(bool visible);
    void setSettingsOpen(bool open);
    void setPipelineStatus(const QString& status);
    void setHasArea(bool hasArea);
    void setSelectionMode(bool on);
    void setDragMode(bool on);

public slots:
    void retranslateUi();

signals:
    void triggerActionRequested();
    void areaSelectRequested();
    void visibilityToggleRequested();
    void stopRequested();
    void settingsToggleRequested();
    void selTranslateRequested();
    void dragModeToggleRequested();

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
    void updateTriggerButton();
    void updateAreaButton();
    void updateSelTransButton();
    void updateDragButton();
    int  expandWidth() const { return m_expandWidth; }
    void setExpandWidth(int w);
    void fitToScreen();

    QPushButton* m_toggleBtn    = nullptr;
    QPushButton* m_triggerBtn   = nullptr;
    QPushButton* m_areaBtn      = nullptr;
    QPushButton* m_stopBtn      = nullptr;
    QPushButton* m_eyeBtn       = nullptr;
    QPushButton* m_settingsBtn  = nullptr;
    QPushButton* m_selTransBtn  = nullptr;
    QPushButton* m_dragBtn      = nullptr;
    QWidget*     m_btnContainer = nullptr;

    QPropertyAnimation* m_anim = nullptr;
    QTimer* m_pulseTimer = nullptr;     // pulse the toggle btn during translating
    int  m_expandWidth   = 44;
    int  m_fullWidth     = 344;
    bool m_expanded      = false;
    Mode m_currentMode   = Mode::Snapshot;
    bool m_visible       = true;
    bool m_settingsOpen  = false;
    bool m_hasArea       = false;
    bool m_selModeActive = false;
    bool m_dragModeActive = false;
    bool m_dragging      = false;
    QPoint m_dragStartPos;
    QPoint m_dragStartGeo;

    QTimer m_hoverDelay;
    static constexpr int kBtnSize    = 36;
    static constexpr int kGap        = 4;
    static constexpr int kMargin     = 4;
    static constexpr int kCollapsedW = 44;
};
