#include "TrayManager.h"
#include "common/LanguageManager.h"
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

    // Mode submenu
    m_modeMenu = new QMenu(tr("Mode"), m_menu);

    m_realtimeAction = m_modeMenu->addAction(tr("Real-time Translation"));
    m_realtimeAction->setCheckable(true);
    m_snapshotAction = m_modeMenu->addAction(tr("Snapshot Translation"));
    m_snapshotAction->setCheckable(true);
    m_pauseAction    = m_modeMenu->addAction(tr("Pause Translation"));
    m_pauseAction->setCheckable(true);

    m_menu->addMenu(m_modeMenu);

    // Language submenu
    m_langMenu = new QMenu(tr("Language"), m_menu);
    m_langEnAction = m_langMenu->addAction(tr("English"));
    m_langEnAction->setCheckable(true);
    m_langZhAction = m_langMenu->addAction(tr("简体中文"));
    m_langZhAction->setCheckable(true);
    m_menu->addMenu(m_langMenu);

    m_menu->addSeparator();

    m_areaAction   = m_menu->addAction(tr("Select Translation Area..."));
    m_toggleAction = m_menu->addAction(tr("Show/Hide All Translations"));
    m_menu->addSeparator();

    m_settingsAction = m_menu->addAction(tr("Settings..."));
    m_menu->addSeparator();

    m_exitAction = m_menu->addAction(tr("Exit"));

    // Mode connections
    connect(m_realtimeAction, &QAction::triggered, this, [this]() {
        emit modeChangeRequested(Mode::RealTime);
    });
    connect(m_snapshotAction, &QAction::triggered, this, [this]() {
        emit modeChangeRequested(Mode::Snapshot);
    });
    connect(m_pauseAction, &QAction::triggered, this, [this]() {
        emit modeChangeRequested(Mode::Pause);
    });

    // Language connections
    connect(m_langEnAction, &QAction::triggered, this, [this]() {
        emit languageChangeRequested("en");
    });
    connect(m_langZhAction, &QAction::triggered, this, [this]() {
        emit languageChangeRequested("zh_CN");
    });

    connect(m_areaAction, &QAction::triggered, this, &TrayManager::areaSelectRequested);
    connect(m_toggleAction, &QAction::triggered, this, &TrayManager::globalVisibilityToggleRequested);
    connect(m_settingsAction, &QAction::triggered, this, &TrayManager::settingsRequested);
    connect(m_exitAction, &QAction::triggered, this, &TrayManager::exitRequested);

    // Sync language checkmarks on language change
    connect(LanguageManager::instance(), &LanguageManager::languageChanged,
            this, &TrayManager::retranslateUi);
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

void TrayManager::retranslateUi() {
    m_langEnAction->setChecked(LanguageManager::instance()->currentLanguage() == "en");
    m_langZhAction->setChecked(LanguageManager::instance()->currentLanguage() == "zh_CN");

    m_modeMenu->setTitle(tr("Mode"));
    m_realtimeAction->setText(tr("Real-time Translation"));
    m_snapshotAction->setText(tr("Snapshot Translation"));
    m_pauseAction->setText(tr("Pause Translation"));
    m_langMenu->setTitle(tr("Language"));
    m_langEnAction->setText(tr("English"));
    m_langZhAction->setText(tr("简体中文"));
    m_areaAction->setText(tr("Select Translation Area..."));
    m_toggleAction->setText(tr("Show/Hide All Translations"));
    m_settingsAction->setText(tr("Settings..."));
    m_exitAction->setText(tr("Exit"));
}
