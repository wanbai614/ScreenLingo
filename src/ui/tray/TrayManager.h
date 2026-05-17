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

signals:
    void modeChangeRequested(Mode mode);
    void startRequested();
    void pauseRequested();
    void areaSelectRequested();
    void globalVisibilityToggleRequested();
    void settingsRequested();
    void languageChangeRequested(const QString& lang);
    void exitRequested();

private:
    void buildMenu();
    void updateModeCheck(Mode mode);

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu*           m_menu     = nullptr;
    QMenu*           m_modeMenu = nullptr;
    QMenu*           m_langMenu = nullptr;
    QAction*         m_realtimeAction = nullptr;
    QAction*         m_snapshotAction = nullptr;
    QAction*         m_startAction    = nullptr;
    QAction*         m_pauseAction    = nullptr;
    QAction*         m_langEnAction   = nullptr;
    QAction*         m_langZhAction   = nullptr;
    QAction*         m_areaAction     = nullptr;
    QAction*         m_toggleAction   = nullptr;
    QAction*         m_settingsAction = nullptr;
    QAction*         m_exitAction     = nullptr;
};
