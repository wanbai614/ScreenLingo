#include "SettingsPanel.h"
#include "core/translate/ITranslator.h"
#include "engine/hotkey/HotkeyManager.h"
#include "common/Config.h"
#include "common/LanguageManager.h"
#include "app/SignalBus.h"
#include "KeyCaptureDialog.h"
#include <QtCore/QPair>
#include <QtGui/QFontDatabase>
#include <QtWidgets/QMessageBox>

SettingsPanel::SettingsPanel(const QVector<ITranslator*>& translators,
                               HotkeyManager* hotkeyMgr,
                               Config* config,
                               SignalBus* bus,
                               const QVector<SelectionArea>* areas,
                               QWidget* parent)
    : QDialog(parent), m_bus(bus), m_config(config),
      m_hotkeyMgr(hotkeyMgr), m_translators(translators),
      m_areasPtr(areas) {

    setWindowTitle(tr("ScreenLingo Settings"));
    setMinimumSize(540, 460);

    m_tabWidget = new QTabWidget;

    m_tabWidget->addTab(createAppearanceTab(),  tr("Appearance"));
    m_tabWidget->addTab(createTranslationTab(), tr("Translation"));
    m_tabWidget->addTab(createHotkeysTab(),     tr("Hotkeys"));
    m_tabWidget->addTab(createAreasTab(),       tr("Areas"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tabWidget);

    // General section
    auto* generalGroup = new QGroupBox(tr("General"));
    auto* generalForm = new QFormLayout(generalGroup);
    m_langCombo = new QComboBox;
    QVector<QPair<QString, QString>> langs = LanguageManager::instance()->availableLanguages();
    for (const auto& pair : langs) {
        m_langCombo->addItem(pair.second, pair.first);
    }
    QString curLang = LanguageManager::instance()->currentLanguage();
    int idx = m_langCombo->findData(curLang);
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        QString lang = m_langCombo->itemData(i).toString();
        emit languageChangeRequested(lang);
    });
    generalForm->addRow(tr("Interface Language:"), m_langCombo);
    layout->addWidget(generalGroup);

    // Buttons
    auto* btnLayout = new QHBoxLayout;
    m_resetBtn  = new QPushButton(tr("Reset Defaults"));
    m_applyBtn  = new QPushButton(tr("Apply"));
    m_closeBtn  = new QPushButton(tr("Close"));
    btnLayout->addWidget(m_resetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_applyBtn);
    btnLayout->addWidget(m_closeBtn);
    layout->addLayout(btnLayout);

    connect(m_applyBtn,  &QPushButton::clicked, this, &SettingsPanel::applyStyle);
    connect(m_closeBtn,  &QPushButton::clicked, this, &QDialog::close);
    // Auto-save when closing the dialog
    connect(this, &QDialog::finished, this, &SettingsPanel::applyStyle);
    connect(m_resetBtn,  &QPushButton::clicked, this, [this]() {
        m_pendingStyle = StyleConfig{};
        loadStyle();
        updatePreview();
    });

    // React to language changes
    connect(LanguageManager::instance(), &LanguageManager::languageChanged,
            this, &SettingsPanel::retranslateUi);

    loadStyle();
}

QWidget* SettingsPanel::createAppearanceTab() {
    auto* page = new QWidget;
    auto* form = new QFormLayout(page);

    m_textColorBtn = new QPushButton;
    m_textColorBtn->setFixedSize(40, 24);
    connect(m_textColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_pendingStyle.textColor, this);
        if (c.isValid()) {
            m_pendingStyle.textColor = c;
            m_textColorBtn->setStyleSheet(
                QString("background-color: %1; border: 1px solid #666;").arg(c.name()));
            updatePreview();
        }
    });
    form->addRow(tr("Text Color:"), m_textColorBtn);

    m_fontSizeSpin = new QSpinBox;
    m_fontSizeSpin->setRange(8, 48);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.font.setPointSize(v);
        updatePreview();
    });
    form->addRow(tr("Font Size:"), m_fontSizeSpin);

    m_fontCombo = new QComboBox;
    QFontDatabase fontDb;
    m_fontCombo->addItems(fontDb.families());
    connect(m_fontCombo, &QComboBox::currentTextChanged, this, [this](const QString& f) {
        m_pendingStyle.font.setFamily(f);
        updatePreview();
    });
    form->addRow(tr("Font:"), m_fontCombo);

    m_bgColorBtn = new QPushButton;
    m_bgColorBtn->setFixedSize(40, 24);
    connect(m_bgColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_pendingStyle.backgroundColor, this,
                                          tr("Choose"), QColorDialog::ShowAlphaChannel);
        if (c.isValid()) {
            m_pendingStyle.backgroundColor = c;
            m_bgColorBtn->setStyleSheet(
                QString("background-color: %1; border: 1px solid #666;").arg(c.name()));
            updatePreview();
        }
    });
    form->addRow(tr("Background:"), m_bgColorBtn);

    m_alphaSlider = new QSlider(Qt::Horizontal);
    m_alphaSlider->setRange(0, 100);
    connect(m_alphaSlider, &QSlider::valueChanged, this, [this](int v) {
        m_pendingStyle.backgroundAlpha = v;
        updatePreview();
    });
    form->addRow(tr("Opacity:"), m_alphaSlider);

    m_borderRadiusSpin = new QSpinBox;
    m_borderRadiusSpin->setRange(0, 20);
    connect(m_borderRadiusSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.borderRadius = v;
        updatePreview();
    });
    form->addRow(tr("Border Radius:"), m_borderRadiusSpin);

    m_borderColorBtn = new QPushButton;
    m_borderColorBtn->setFixedSize(40, 24);
    connect(m_borderColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_pendingStyle.borderColor, this);
        if (c.isValid()) {
            m_pendingStyle.borderColor = c;
            m_borderColorBtn->setStyleSheet(
                QString("background-color: %1; border: 1px solid #666;").arg(c.name()));
            updatePreview();
        }
    });
    form->addRow(tr("Border Color:"), m_borderColorBtn);

    m_borderWidthSpin = new QSpinBox;
    m_borderWidthSpin->setRange(0, 5);
    connect(m_borderWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.borderWidth = v;
        updatePreview();
    });
    form->addRow(tr("Border Width:"), m_borderWidthSpin);

    auto* previewGroup = new QGroupBox(tr("Preview"));
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(tr("Hello World!"));
    m_previewLabel->setMinimumHeight(50);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_previewLabel);
    form->addRow(previewGroup);

    return page;
}

QWidget* SettingsPanel::createTranslationTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    m_serviceCombo = new QComboBox;
    for (auto* t : m_translators) {
        m_serviceCombo->addItem(
            QString("%1 (%2)").arg(t->name(), t->category()), t->name());
    }
    layout->addWidget(new QLabel(tr("Active Service:")));
    layout->addWidget(m_serviceCombo);

    m_translatorConfigLayout = new QVBoxLayout;
    layout->addLayout(m_translatorConfigLayout);

    connect(m_serviceCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        QLayoutItem* item;
        while ((item = m_translatorConfigLayout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }

        auto* translator = m_translators.value(idx);
        if (!translator) return;

        auto* group = new QGroupBox(translator->name() + " " + tr("Configuration"));
        auto* form = new QFormLayout(group);

        for (const auto& field : translator->configFields()) {
            auto* edit = new QLineEdit;
            edit->setText(translator->getConfig(field.key));
            if (field.isSecret) edit->setEchoMode(QLineEdit::Password);

            connect(edit, &QLineEdit::textChanged, this, [translator, key = field.key](const QString& v) {
                translator->setConfig(key, v);
            });

            form->addRow(field.label + ":", edit);
        }

        m_translatorConfigLayout->addWidget(group);
    });

    if (m_translators.size() > 0) {
        // Restore saved active translator
        QString saved = m_config->activeTranslator();
        int idx = m_serviceCombo->findData(saved);
        if (idx >= 0)
            m_serviceCombo->setCurrentIndex(idx);
        else
            m_serviceCombo->setCurrentIndex(0);
    }

    layout->addStretch();
    return page;
}

QWidget* SettingsPanel::createHotkeysTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel(tr("Hotkey bindings:")));

    for (const auto& binding : m_hotkeyMgr->bindings()) {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(binding.label));
        auto* keyBtn = new QPushButton(binding.currentKeys);
        keyBtn->setMinimumWidth(120);

        connect(keyBtn, &QPushButton::clicked, this, [this, b = binding, keyBtn]() {
            KeyCaptureDialog dlg(b.currentKeys, this);
            if (dlg.exec() == QDialog::Accepted) {
                QString newKeys = dlg.capturedKeys();
                if (!newKeys.isEmpty() && newKeys != b.currentKeys) {
                    // Check for conflicts
                    if (m_hotkeyMgr->hasConflict(newKeys, b.id)) {
                        QMessageBox::warning(this, tr("Conflict"),
                            tr("Shortcut '%1' is already in use.").arg(newKeys));
                        return;
                    }
                    m_hotkeyMgr->updateBinding(b.id, newKeys);
                    keyBtn->setText(newKeys);
                    m_config->saveHotkeys(m_hotkeyMgr->bindings());
                }
            }
        });

        row->addWidget(keyBtn);
        row->addStretch();
        layout->addLayout(row);
    }

    auto* resetBtn = new QPushButton(tr("Reset All to Defaults"));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        for (auto& b : m_hotkeyMgr->bindings()) {
            m_hotkeyMgr->updateBinding(b.id, b.defaultKeys);
        }
        m_config->saveHotkeys(m_hotkeyMgr->bindings());
        // Refresh UI: rebuild the hotkeys tab
        retranslateUi();
    });
    layout->addWidget(resetBtn);
    layout->addStretch();
    return page;
}

QWidget* SettingsPanel::createAreasTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel(tr("Translation Areas:")));

    // Area info display
    m_areaInfoLabel = new QLabel;
    m_areaInfoLabel->setWordWrap(true);
    m_areaInfoLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_areaInfoLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_areaInfoLabel->setMinimumHeight(80);
    layout->addWidget(m_areaInfoLabel);

    // Enabled checkbox
    m_areaEnableCb = new QCheckBox(tr("Enable this area"));
    connect(m_areaEnableCb, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_areasPtr && !m_areasPtr->isEmpty()) {
            emit areaEnabledChanged(m_areasPtr->first().id, checked);
        }
    });
    layout->addWidget(m_areaEnableCb);

    // Action buttons
    auto* btnRow = new QHBoxLayout;
    m_areaReselectBtn = new QPushButton(tr("Select Area (Ctrl+Shift+A)"));
    connect(m_areaReselectBtn, &QPushButton::clicked, this, &SettingsPanel::areaSelectRequested);
    btnRow->addWidget(m_areaReselectBtn);

    m_areaClearBtn = new QPushButton(tr("Clear Area"));
    connect(m_areaClearBtn, &QPushButton::clicked, this, [this]() {
        emit areaCleared();
        refreshAreas();
    });
    btnRow->addWidget(m_areaClearBtn);
    layout->addLayout(btnRow);

    layout->addStretch();

    refreshAreas();
    return page;
}

void SettingsPanel::refreshAreas() {
    if (!m_areaInfoLabel || !m_areaEnableCb || !m_areaClearBtn) return;

    if (!m_areasPtr || m_areasPtr->isEmpty()) {
        m_areaInfoLabel->setText(tr("No areas configured.\n"
                                    "Use Ctrl+Shift+A or the tray menu to select an area.\n\n"
                                    "Select a translation area on screen — ScreenLingo will "
                                    "only OCR and translate text within this region."));
        m_areaInfoLabel->setStyleSheet("color: #888; background: #f5f5f5; padding: 8px;");
        m_areaEnableCb->setChecked(false);
        m_areaEnableCb->setEnabled(false);
        m_areaClearBtn->setEnabled(false);
        m_areaReselectBtn->setText(tr("Select Area (Ctrl+Shift+A)"));
        return;
    }

    const auto& area = m_areasPtr->first();
    m_areaEnableCb->setEnabled(true);
    m_areaEnableCb->setChecked(area.enabled);
    m_areaClearBtn->setEnabled(true);
    m_areaReselectBtn->setText(tr("Reselect Area"));

    m_areaInfoLabel->setStyleSheet("color: #333; background: #e8f5e9; padding: 8px;");
    m_areaInfoLabel->setText(QString(
        "Area #%1  |  Screen %2\n"
        "Position: (%3, %4)\n"
        "Size: %5 x %6  (%7 x %8 px)")
        .arg(area.id)
        .arg(area.screenIndex)
        .arg(area.geometry.x()).arg(area.geometry.y())
        .arg(area.geometry.width()).arg(area.geometry.height())
        .arg(area.geometry.width()).arg(area.geometry.height()));
}

void SettingsPanel::loadStyle() {
    m_pendingStyle = m_config->loadStyle();

    m_textColorBtn->setStyleSheet(
        QString("background-color: %1; border: 1px solid #666;").arg(m_pendingStyle.textColor.name()));
    m_fontSizeSpin->setValue(m_pendingStyle.font.pointSize());
    int idx = m_fontCombo->findText(m_pendingStyle.font.family());
    if (idx >= 0) m_fontCombo->setCurrentIndex(idx);
    m_bgColorBtn->setStyleSheet(
        QString("background-color: %1; border: 1px solid #666;").arg(m_pendingStyle.backgroundColor.name()));
    m_alphaSlider->setValue(m_pendingStyle.backgroundAlpha);
    m_borderRadiusSpin->setValue(m_pendingStyle.borderRadius);
    m_borderColorBtn->setStyleSheet(
        QString("background-color: %1; border: 1px solid #666;").arg(m_pendingStyle.borderColor.name()));
    m_borderWidthSpin->setValue(m_pendingStyle.borderWidth);

    updatePreview();
}

void SettingsPanel::updatePreview() {
    QColor bg = m_pendingStyle.backgroundColor;
    bg.setAlpha(static_cast<int>(m_pendingStyle.backgroundAlpha * 255 / 100));
    m_previewLabel->setStyleSheet(QString(
        "background-color: %1; color: %2; border: %3px solid %4; border-radius: %5px; "
        "font-family: \"%6\"; font-size: %7pt; padding: 8px;")
        .arg(bg.name(QColor::HexArgb))
        .arg(m_pendingStyle.textColor.name())
        .arg(m_pendingStyle.borderWidth)
        .arg(m_pendingStyle.borderColor.name())
        .arg(m_pendingStyle.borderRadius)
        .arg(m_pendingStyle.font.family())
        .arg(m_pendingStyle.font.pointSize()));
}

void SettingsPanel::applyStyle() {
    m_config->saveStyle(m_pendingStyle);

    QString activeName = m_serviceCombo->currentData().toString();
    m_config->setActiveTranslator(activeName);

    for (auto* t : m_translators) {
        for (const auto& field : t->configFields()) {
            m_config->setTranslatorConfig(t->name(), field.key, t->getConfig(field.key));
        }
    }

    // Save language preference
    QString lang = m_langCombo->currentData().toString();
    m_config->setTranslatorConfig("app", "language", lang);

    emit styleChanged(m_pendingStyle);
    if (m_bus) m_bus->styleChanged(m_pendingStyle);
}

void SettingsPanel::retranslateUi() {
    setWindowTitle(tr("ScreenLingo Settings"));
    m_tabWidget->setTabText(0, tr("Appearance"));
    m_tabWidget->setTabText(1, tr("Translation"));
    m_tabWidget->setTabText(2, tr("Hotkeys"));
    m_tabWidget->setTabText(3, tr("Areas"));
    m_resetBtn->setText(tr("Reset Defaults"));
    m_applyBtn->setText(tr("Apply"));
    m_closeBtn->setText(tr("Close"));
    updatePreview();
}
