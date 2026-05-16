#include "TrayManager.h"
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

TrayManager::TrayManager(QObject* parent) : QObject(parent) {}

void TrayManager::initialize() {
    m_trayIcon = new QSystemTrayIcon(this);

    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(0, 180, 120));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(6, 6, 20, 20, 4, 4);
    painter.end();

    m_trayIcon->setIcon(QIcon(pix));
    m_trayIcon->setToolTip("ScreenLingo");

    buildMenu();
    m_trayIcon->setContextMenu(m_menu);
    m_trayIcon->show();
}

void TrayManager::buildMenu() {
    m_menu = new QMenu();

    m_modeMenu = new QMenu("Mode", m_menu);

    m_realtimeAction = m_modeMenu->addAction("Real-time Translation");
    m_realtimeAction->setCheckable(true);
    m_snapshotAction = m_modeMenu->addAction("Snapshot Translation");
    m_snapshotAction->setCheckable(true);
    m_pauseAction    = m_modeMenu->addAction("Pause Translation");
    m_pauseAction->setCheckable(true);

    m_menu->addMenu(m_modeMenu);
    m_menu->addSeparator();

    QAction* areaAction = m_menu->addAction("Select Translation Area...");
    QAction* toggleAction = m_menu->addAction("Show/Hide All Translations");
    m_menu->addSeparator();

    QAction* settingsAction = m_menu->addAction("Settings...");
    m_menu->addSeparator();

    QAction* exitAction = m_menu->addAction("Exit");

    connect(m_realtimeAction, &QAction::triggered, this, [this]() {
        emit modeChangeRequested(Mode::RealTime);
    });
    connect(m_snapshotAction, &QAction::triggered, this, [this]() {
        emit modeChangeRequested(Mode::Snapshot);
    });
    connect(m_pauseAction, &QAction::triggered, this, [this]() {
        emit modeChangeRequested(Mode::Pause);
    });
    connect(areaAction, &QAction::triggered, this, &TrayManager::areaSelectRequested);
    connect(toggleAction, &QAction::triggered, this, &TrayManager::globalVisibilityToggleRequested);
    connect(settingsAction, &QAction::triggered, this, &TrayManager::settingsRequested);
    connect(exitAction, &QAction::triggered, this, &TrayManager::exitRequested);
}

void TrayManager::updateModeCheck(Mode mode) {
    m_realtimeAction->setChecked(mode == Mode::RealTime);
    m_snapshotAction->setChecked(mode == Mode::Snapshot);
    m_pauseAction->setChecked(mode == Mode::Pause);
}

void TrayManager::updateIcon(Mode mode) {
    updateModeCheck(mode);

    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    if (mode == Mode::Pause) {
        painter.setBrush(QColor(200, 160, 0));
    } else {
        painter.setBrush(QColor(0, 180, 120));
    }
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(6, 6, 20, 20, 4, 4);
    painter.end();

    m_trayIcon->setIcon(QIcon(pix));
}
