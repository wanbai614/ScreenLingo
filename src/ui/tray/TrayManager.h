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

signals:
    void modeChangeRequested(Mode mode);
    void areaSelectRequested();
    void globalVisibilityToggleRequested();
    void settingsRequested();
    void exitRequested();

private:
    void buildMenu();
    void updateModeCheck(Mode mode);

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu*           m_menu     = nullptr;
    QMenu*           m_modeMenu = nullptr;
    QAction*         m_realtimeAction = nullptr;
    QAction*         m_snapshotAction = nullptr;
    QAction*         m_pauseAction    = nullptr;
};
