#pragma once

#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtCore/QObject>
#include "common/Types.h"

class TrayManager : public QObject {
    Q_OBJECT

public:
    explicit TrayManager(QObject* parent = nullptr);
    void initialize();
    void updateIcon(Mode mode);

public slots:
    void retranslateUi();
    void setGlobalVisible(bool visible);
    void setSelectionMode(bool on);

signals:
    void modeChangeRequested(Mode mode);
    void triggerActionRequested();  // snapshot: translate once, realtime→pause, pause→realtime
    void areaSelectRequested();
    void editAreaRequested();
    void globalVisibilityToggleRequested();
    void settingsRequested();
    void languageChangeRequested(const QString& lang);
    void stopRequested();
    void exitRequested();
    void selTranslateRequested();

private:
    void buildMenu();
    void updateModeCheck(Mode mode);

    Mode m_currentMode = Mode::Snapshot;

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu*           m_menu     = nullptr;
    QMenu*           m_modeMenu = nullptr;
    QMenu*           m_langMenu = nullptr;
    QAction*         m_realtimeAction = nullptr;
    QAction*         m_snapshotAction = nullptr;
    QAction*         m_triggerAction  = nullptr;   // dynamic: Start/Pause/Snapshot
    QAction*         m_langEnAction   = nullptr;
    QAction*         m_langZhAction   = nullptr;
    QAction*         m_areaAction     = nullptr;
    QAction*         m_editAreaAction = nullptr;
    QAction*         m_toggleAction   = nullptr;
    QAction*         m_stopAction     = nullptr;
    QAction*         m_selTransAction = nullptr;
    QAction*         m_settingsAction = nullptr;
    QAction*         m_exitAction     = nullptr;
    bool             m_globalVisible = true;
    bool             m_selModeActive = false;
};
