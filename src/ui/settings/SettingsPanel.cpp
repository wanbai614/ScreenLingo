#include "SettingsPanel.h"
#include "core/translate/ITranslator.h"
#include "engine/hotkey/HotkeyManager.h"
#include "common/Config.h"
#include "common/LanguageManager.h"
#include "app/SignalBus.h"
#include "KeyCaptureDialog.h"
#include "PromptManagerDialog.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QPair>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtGui/QFontDatabase>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <memory>

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
    setMinimumSize(640, 580);

    m_tabWidget = new QTabWidget;

    m_tabWidget->addTab(createGeneralTab(),      tr("General"));
    m_tabWidget->addTab(createTranslationTab(),  tr("Translation"));
    m_tabWidget->addTab(createHotkeysTab(),      tr("Hotkeys"));
    m_tabWidget->addTab(createAreasTab(),        tr("Areas"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tabWidget);

    // Buttons
    auto* btnLayout = new QHBoxLayout;
    m_resetBtn  = new QPushButton(tr("Reset Defaults"));
    m_applyBtn  = new QPushButton(tr("Apply"));
    m_closeBtn  = new QPushButton(tr("Close"));
    m_applyBtn->setObjectName("applyBtn");
    m_closeBtn->setObjectName("closeBtn");
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
    applySettingsTheme();
}

QWidget* SettingsPanel::createGeneralTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    // ── System ──
    m_generalGroup = new QGroupBox(tr("System"));
    m_i18nGroups.append({m_generalGroup, "System"});
    auto* systemForm = new QFormLayout(m_generalGroup);

    m_langCombo = new QComboBox;
    QVector<QPair<QString, QString>> langs = LanguageManager::instance()->availableLanguages();
    for (const auto& pair : langs)
        m_langCombo->addItem(pair.second, pair.first);
    QString curLang = LanguageManager::instance()->currentLanguage();
    int idx = m_langCombo->findData(curLang);
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        emit languageChangeRequested(m_langCombo->itemData(i).toString());
    });
    m_langLabel = new QLabel(tr("Interface Language:"));
    m_i18nLabels.append({m_langLabel, "Interface Language:"});
    systemForm->addRow(m_langLabel, m_langCombo);

    m_themeCombo = new QComboBox;
    m_themeCombo->addItem(tr("Dark"),  "dark");
    m_themeCombo->addItem(tr("Light"), "light");
    QString savedTheme = m_config->theme();
    int themeIdx = m_themeCombo->findData(savedTheme);
    if (themeIdx >= 0) m_themeCombo->setCurrentIndex(themeIdx);
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_config->setTheme(m_themeCombo->currentData().toString());
        applySettingsTheme();
    });
    auto* themeLabel = new QLabel(tr("Theme:"));
    m_i18nLabels.append({themeLabel, "Theme:"});
    systemForm->addRow(themeLabel, m_themeCombo);

    m_ocrCombo = new QComboBox;
    m_ocrCombo->addItem("Windows OCR (built-in)", "windows");
#if HAS_PADDLE_OCR
    m_ocrCombo->addItem("PaddleOCR (offline, high accuracy)", "paddle");
#endif
    m_ocrCombo->addItem("GLM-OCR (cloud, GLM-4V vision)", "glmocr");
    QString savedOCR = m_config->ocrEngine();
    int ocrIdx = m_ocrCombo->findData(savedOCR);
    if (ocrIdx >= 0) m_ocrCombo->setCurrentIndex(ocrIdx);
    connect(m_ocrCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_config->setOCREngine(m_ocrCombo->currentData().toString());
    });
    m_ocrLabel = new QLabel(tr("OCR Engine:"));
    m_i18nLabels.append({m_ocrLabel, "OCR Engine:"});
    systemForm->addRow(m_ocrLabel, m_ocrCombo);
    layout->addWidget(m_generalGroup);

    // ── Bubble Appearance ──
    auto* appearGroup = new QGroupBox(tr("Bubble Appearance"));
    m_i18nGroups.append({appearGroup, "Bubble Appearance"});
    auto* form = new QFormLayout(appearGroup);

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
    auto i18nRow = [&](const char* en, QWidget* w) {
        auto* lbl = new QLabel(tr(en));
        m_i18nLabels.append({lbl, en});
        form->addRow(lbl, w);
    };
    i18nRow("Text Color:", m_textColorBtn);

    m_fontSizeSpin = new QSpinBox;
    m_fontSizeSpin->setRange(8, 48);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.font.setPointSize(v);
        updatePreview();
    });
    i18nRow("Font Size:", m_fontSizeSpin);

    m_fontCombo = new QComboBox;
    QFontDatabase fontDb;
    m_fontCombo->addItems(fontDb.families());
    connect(m_fontCombo, &QComboBox::currentTextChanged, this, [this](const QString& f) {
        m_pendingStyle.font.setFamily(f);
        updatePreview();
    });
    i18nRow("Font:", m_fontCombo);

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
    i18nRow("Background:", m_bgColorBtn);

    m_alphaSlider = new QSlider(Qt::Horizontal);
    m_alphaSlider->setRange(0, 100);
    connect(m_alphaSlider, &QSlider::valueChanged, this, [this](int v) {
        m_pendingStyle.backgroundAlpha = v;
        updatePreview();
    });
    i18nRow("Opacity:", m_alphaSlider);

    m_borderRadiusSpin = new QSpinBox;
    m_borderRadiusSpin->setRange(0, 20);
    connect(m_borderRadiusSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.borderRadius = v;
        updatePreview();
    });
    i18nRow("Border Radius:", m_borderRadiusSpin);

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
    i18nRow("Border Color:", m_borderColorBtn);

    m_borderWidthSpin = new QSpinBox;
    m_borderWidthSpin->setRange(0, 5);
    connect(m_borderWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.borderWidth = v;
        updatePreview();
    });
    i18nRow("Border Width:", m_borderWidthSpin);
    layout->addWidget(appearGroup);

    // ── Preview ──
    auto* previewGroup = new QGroupBox(tr("Preview"));
    m_i18nGroups.append({previewGroup, "Preview"});
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(tr("Hello World!"));
    m_i18nLabels.append({m_previewLabel, "Hello World!"});
    m_previewLabel->setMinimumHeight(50);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_previewLabel);
    layout->addWidget(previewGroup);

    layout->addStretch();
    return page;
}

QWidget* SettingsPanel::createTranslationTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    // ── Language Pair ──
    auto* langGroup = new QGroupBox(tr("Language Pair"));
    m_i18nGroups.append({langGroup, "Language Pair"});
    auto* langForm = new QFormLayout(langGroup);

    // Language list
    struct LangEntry { QString code; QString name; };
    QVector<LangEntry> langList = {
        {"auto", tr("Auto Detect")},
        {"zh",   tr("Chinese")},
        {"en",   tr("English")},
        {"ja",   tr("Japanese")},
        {"ko",   tr("Korean")},
        {"fr",   tr("French")},
        {"de",   tr("German")},
        {"es",   tr("Spanish")},
        {"pt",   tr("Portuguese")},
        {"ru",   tr("Russian")},
        {"ar",   tr("Arabic")},
        {"th",   tr("Thai")},
        {"vi",   tr("Vietnamese")},
        {"it",   tr("Italian")},
        {"nl",   tr("Dutch")},
        {"pl",   tr("Polish")},
    };

    m_srcLangCombo = new QComboBox;
    m_tgtLangCombo = new QComboBox;
    for (const auto& l : langList) {
        m_srcLangCombo->addItem(l.name, l.code);
        if (l.code != "auto")  // target can't be "auto"
            m_tgtLangCombo->addItem(l.name, l.code);
    }
    QString savedSrc = m_config->sourceLang();
    QString savedTgt = m_config->targetLang();
    int si = m_srcLangCombo->findData(savedSrc);
    int ti = m_tgtLangCombo->findData(savedTgt);
    if (si >= 0) m_srcLangCombo->setCurrentIndex(si);
    if (ti >= 0) m_tgtLangCombo->setCurrentIndex(ti);

    connect(m_srcLangCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_config->setSourceLang(m_srcLangCombo->currentData().toString());
    });
    connect(m_tgtLangCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_config->setTargetLang(m_tgtLangCombo->currentData().toString());
    });

    auto* srcLabel = new QLabel(tr("From:"));
    auto* tgtLabel = new QLabel(tr("To:"));
    m_i18nLabels.append({srcLabel, "From:"});
    m_i18nLabels.append({tgtLabel, "To:"});
    langForm->addRow(srcLabel, m_srcLangCombo);
    langForm->addRow(tgtLabel, m_tgtLangCombo);
    layout->addWidget(langGroup);

    // Helper: build config panel for a given translator
    auto showConfig = [this](ITranslator* translator) {
        QLayoutItem* item;
        while ((item = m_translatorConfigLayout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        if (!translator) return;

        auto* group = new QGroupBox(translator->name() + " " + tr("Configuration"));
        m_translatorConfigGroup = group;
        auto* form = new QFormLayout(group);
        for (const auto& field : translator->configFields()) {
            // System prompt is now managed by Translator Persona presets
            if (field.key == QStringLiteral("systemPrompt")) continue;
            auto* edit = new QLineEdit;
            edit->setText(translator->getConfig(field.key));
            if (field.isSecret) edit->setEchoMode(QLineEdit::Password);
            connect(edit, &QLineEdit::textChanged, this,
                [translator, key = field.key](const QString& v) {
                    translator->setConfig(key, v);
                });
            if (field.key == QStringLiteral("downloadUrl")) {
                auto* row = new QWidget;
                auto* rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(0, 0, 0, 0);
                rowLayout->addWidget(edit, 1);
                auto* dlBtn = new QPushButton(tr("Download"));
                connect(dlBtn, &QPushButton::clicked, this, [translator]() {
                    QMetaObject::invokeMethod(translator, "downloadDictionary");
                });
                rowLayout->addWidget(dlBtn);
                form->addRow(field.label + ":", row);
            } else {
                form->addRow(field.label + ":", edit);
            }
        }
        m_translatorConfigLayout->addWidget(group);
    };

    // Compact category picker: label + combo + active dot, in one row
    auto makeCategoryRow = [&](const QString& category, QComboBox*& combo,
                                const char* label) -> QWidget* {
        auto* row = new QWidget;
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(0, 1, 0, 1);
        auto* lbl = new QLabel(tr(label));
        m_i18nLabels.append({lbl, label});
        lbl->setFixedWidth(60);
        lbl->setStyleSheet("font-weight: 600; color: #8b92a5; font-size: 12px;");
        hbox->addWidget(lbl);
        combo = new QComboBox;
        combo->setMinimumWidth(130);
        for (auto* t : m_translators)
            if (t->category() == category)
                combo->addItem(t->name(), t->name());
        hbox->addWidget(combo);
        auto* dot = new QLabel;
        dot->setFixedWidth(14);
        dot->setStyleSheet("color: #1a73e8; font-weight: bold;");
        dot->hide();
        hbox->addWidget(dot);
        hbox->addStretch();

        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, combo, showConfig, dot](int) {
                QString name = combo->currentData().toString();
                m_config->setActiveTranslator(name);
                emit translatorChangeRequested(name);
                ITranslator* t = nullptr;
                for (auto* tr : m_translators)
                    if (tr->name() == name) { t = tr; break; }
                showConfig(t);
                // VLM only works with LLM — auto-uncheck for online/local
                if (t && t->category() != "llm" && m_vlmCb && m_vlmCb->isChecked()) {
                    m_vlmCb->setChecked(false);
                    m_config->setVlmSnapshot(false);
                }
                for (auto* other : {m_onlineCombo, m_llmCombo, m_localCombo}) {
                    if (other && other != combo) {
                        other->blockSignals(true);
                        int i = other->findData(name);
                        if (i >= 0) other->setCurrentIndex(i);
                        else        other->setCurrentIndex(-1);
                        other->blockSignals(false);
                    }
                }
                // Update all dots — only the active combo's shows
                for (auto* oc : {m_onlineCombo, m_llmCombo, m_localCombo}) {
                    if (!oc) continue;
                    auto* pw = oc->parentWidget();
                    if (!pw) continue;
                    auto labels = pw->findChildren<QLabel*>();
                    for (auto* lb : labels)
                        if (lb->styleSheet().contains("#1a73e8"))
                            lb->setVisible(oc == combo);
                }
            });
        return row;
    };

    auto* svcGroup = new QGroupBox(tr("Translation Service"));
    m_i18nGroups.append({svcGroup, "Translation Service"});
    auto* svcLayout = new QVBoxLayout(svcGroup);
    svcLayout->addWidget(makeCategoryRow("online",  m_onlineCombo, "Online:"));
    svcLayout->addWidget(makeCategoryRow("llm",     m_llmCombo,    "LLM:"));
    svcLayout->addWidget(makeCategoryRow("offline", m_localCombo,  "Local:"));
    layout->addWidget(svcGroup);

    m_vlmCb = new QCheckBox(tr("VLM Snapshot (vision model detects + translates)"));
    m_i18nCheckBoxes.append({m_vlmCb, "VLM Snapshot (vision model detects + translates)"});
    m_vlmCb->setChecked(m_config->vlmSnapshot());
    connect(m_vlmCb, &QCheckBox::toggled, this, [this](bool checked) {
        m_config->setVlmSnapshot(checked);
    });
    layout->addWidget(m_vlmCb);

    // UI Translate Mode: no line grouping, each text element independent
    m_uiModeCb = new QCheckBox(tr("UI Mode (no line grouping — for software/menus)"));
    m_i18nCheckBoxes.append({m_uiModeCb, "UI Mode (no line grouping — for software/menus)"});
    m_uiModeCb->setChecked(m_config->uiTranslateMode());
    connect(m_uiModeCb, &QCheckBox::toggled, this, [this](bool checked) {
        m_config->setUITranslateMode(checked);
    });
    layout->addWidget(m_uiModeCb);

    // --- Prompt Preset selector ---
    auto* promptGroup = new QGroupBox(tr("Translator Persona"));
    m_i18nGroups.append({promptGroup, "Translator Persona"});
    auto* promptLayout = new QVBoxLayout(promptGroup);

    auto* promptRow = new QHBoxLayout;
    m_promptCombo = new QComboBox;
    m_promptCombo->setMinimumWidth(200);
    m_promptMgrBtn = new QPushButton(tr("Manage..."));
    m_i18nButtons.append({m_promptMgrBtn, "Manage..."});
    m_promptMgrBtn->setFixedWidth(80);
    promptRow->addWidget(m_promptCombo, 1);
    promptRow->addWidget(m_promptMgrBtn);
    promptLayout->addLayout(promptRow);

    // Description label
    auto* promptDesc = new QLabel;
    promptDesc->setWordWrap(true);
    promptDesc->setStyleSheet("color: #8b92a5; font-size: 11px; padding: 4px 0;");
    promptLayout->addWidget(promptDesc);

    // Load presets into combo
    auto promptPresets = loadOrInitPresets();
    QString activeId = m_config->activePromptId();
    for (const auto& p : promptPresets) {
        m_promptCombo->addItem(p.name, p.id);
        if (p.id == activeId)
            m_promptCombo->setCurrentIndex(m_promptCombo->count() - 1);
    }

    // Update description when selection changes
    auto updatePromptDesc = [promptPresets, promptDesc](const QString& id) {
        for (const auto& p : promptPresets) {
            if (p.id == id) {
                QString desc = p.prompt;
                // Show first 2 lines as preview
                QStringList lines = desc.split('\n');
                QString preview;
                for (int i = 0; i < qMin(2, lines.size()); ++i) {
                    if (!preview.isEmpty()) preview += " ";
                    preview += lines[i].trimmed();
                }
                if (preview.length() > 120)
                    preview = preview.left(120) + "...";
                promptDesc->setText(preview);
                return;
            }
        }
        promptDesc->clear();
    };

    connect(m_promptCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this, updatePromptDesc]() {
            QString id = m_promptCombo->currentData().toString();
            m_config->setActivePromptId(id);
            applyActivePrompt();
            updatePromptDesc(id);
        });
    connect(m_promptMgrBtn, &QPushButton::clicked, this, &SettingsPanel::openPromptManager);

    // Initial description
    if (m_promptCombo->currentIndex() >= 0)
        updatePromptDesc(m_promptCombo->currentData().toString());

    layout->addWidget(promptGroup);

    // --- Config panel ---
    m_translatorConfigLayout = new QVBoxLayout;
    layout->addLayout(m_translatorConfigLayout);

    // Restore saved active translator
    QString saved = m_config->activeTranslator();
    for (auto* combo : {m_onlineCombo, m_llmCombo, m_localCombo}) {
        if (!combo) continue;
        int idx = combo->findData(saved);
        if (idx >= 0) { combo->setCurrentIndex(idx); break; }
    }
    // Show config for first available if saved not found
    ITranslator* active = m_translators.value(0);
    for (auto* t : m_translators)
        if (t->name() == saved) { active = t; break; }
    showConfig(active);

    layout->addStretch();
    return page;
}

QWidget* SettingsPanel::createHotkeysTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    m_hotkeysTitleLabel = new QLabel(tr("Hotkey bindings:"));
    m_i18nLabels.append({m_hotkeysTitleLabel, "Hotkey bindings:"});
    layout->addWidget(m_hotkeysTitleLabel);

    for (const auto& binding : m_hotkeyMgr->bindings()) {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(binding.label);
        m_hotkeyLabelIds.append({lbl, binding.id});
        row->addWidget(lbl);
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

    m_hotkeysResetBtn = new QPushButton(tr("Reset All to Defaults"));
    m_i18nButtons.append({m_hotkeysResetBtn, "Reset All to Defaults"});
    connect(m_hotkeysResetBtn, &QPushButton::clicked, this, [this]() {
        for (auto& b : m_hotkeyMgr->bindings()) {
            m_hotkeyMgr->updateBinding(b.id, b.defaultKeys);
        }
        m_config->saveHotkeys(m_hotkeyMgr->bindings());
        // Refresh UI: rebuild the hotkeys tab
        retranslateUi();
    });
    layout->addWidget(m_hotkeysResetBtn);
    layout->addStretch();
    return page;
}

QWidget* SettingsPanel::createAreasTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    m_areasTitleLabel = new QLabel(tr("Translation Areas:"));
    m_i18nLabels.append({m_areasTitleLabel, "Translation Areas:"});
    layout->addWidget(m_areasTitleLabel);

    // Area info display
    m_areaInfoLabel = new QLabel;
    m_areaInfoLabel->setWordWrap(true);
    m_areaInfoLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_areaInfoLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_areaInfoLabel->setMinimumHeight(80);
    layout->addWidget(m_areaInfoLabel);

    // Enabled checkbox
    m_areaEnableCb = new QCheckBox(tr("Enable this area"));
    m_i18nCheckBoxes.append({m_areaEnableCb, "Enable this area"});
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

    bool isDark = m_themeCombo && m_themeCombo->currentData().toString() == QStringLiteral("dark");

    if (!m_areasPtr || m_areasPtr->isEmpty()) {
        m_areaInfoLabel->setText(tr("No areas configured.\n"
                                    "Use Ctrl+Shift+A or the tray menu to select an area.\n\n"
                                    "Select a translation area on screen — ScreenLingo will "
                                    "only OCR and translate text within this region."));
        m_areaInfoLabel->setStyleSheet(isDark
            ? "color: #6b6f7b; background: rgba(255,255,255,0.03); "
              "border: 1px solid #2a2d37; border-radius: 8px; padding: 12px;"
            : "color: #9ca3af; background: #f9fafb; "
              "border: 1px solid #e5e7eb; border-radius: 8px; padding: 12px;");
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

    m_areaInfoLabel->setStyleSheet(isDark
        ? "color: #a7f0ba; background: rgba(16,185,129,0.08); "
          "border: 1px solid rgba(16,185,129,0.25); border-radius: 8px; padding: 12px;"
        : "color: #065f46; background: rgba(16,185,129,0.06); "
          "border: 1px solid rgba(16,185,129,0.20); border-radius: 8px; padding: 12px;");
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

void SettingsPanel::applySettingsTheme() {
    bool isDark = m_themeCombo->currentData().toString() == QStringLiteral("dark");

    if (isDark) {
        setStyleSheet(QStringLiteral(
            "QDialog { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, "
            "  stop:0 #1a1d24, stop:1 #14161c); }"
            "QTabWidget::pane { border: 1px solid #2a2d36; border-radius: 10px; "
            "  background: #1e2028; margin-top: -1px; padding: 4px; }"
            "QTabBar::tab { padding: 10px 22px; margin-right: 3px; "
            "  border: none; border-bottom: 2px solid transparent; "
            "  background: transparent; color: #6b6f7b; font-weight: 600; font-size: 13px; }"
            "QTabBar::tab:selected { color: #a5b4fc; "
            "  border-bottom: 2px solid #818cf8; background: rgba(129,140,248,0.06); }"
            "QTabBar::tab:hover:!selected { color: #8b92a5; background: rgba(255,255,255,0.03); }"
            "QGroupBox { font-weight: 600; color: #c8cdd8; border: 1px solid #2a2d37; "
            "  border-radius: 10px; margin-top: 14px; padding: 18px 14px 14px 14px; "
            "  background: rgba(255,255,255,0.025); }"
            "QGroupBox::title { subcontrol-origin: margin; left: 16px; "
            "  padding: 0 8px; color: #818cf8; font-size: 12px; }"
            "QPushButton { padding: 7px 18px; border: 1px solid #353845; "
            "  border-radius: 8px; background: #252831; color: #c8cdd8; "
            "  font-weight: 600; font-size: 12px; min-height: 30px; }"
            "QPushButton:hover { background: #2e313d; border-color: #818cf8; color: #e2e5ee; }"
            "QPushButton:pressed { background: #1e2029; border-color: #6366f1; }"
            "QPushButton#applyBtn { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, "
            "  stop:0 #6366f1, stop:1 #818cf8); color: #fff; border: none; "
            "  padding: 9px 28px; font-size: 13px; }"
            "QPushButton#applyBtn:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, "
            "  stop:0 #4f46e5, stop:1 #6366f1); }"
            "QPushButton#closeBtn { background: #2a2d38; border: 1px solid #353845; }"
            "QPushButton#closeBtn:hover { background: #353845; border-color: #ef4444; }"
            "QComboBox { padding: 6px 12px; border: 1px solid #353845; "
            "  border-radius: 8px; background: #252831; color: #c8cdd8; "
            "  min-height: 26px; font-size: 12px; }"
            "QComboBox:hover { border-color: #6366f1; }"
            "QComboBox::drop-down { border: none; width: 26px; }"
            "QComboBox QAbstractItemView { background: #1e2028; border: 1px solid #353845; "
            "  border-radius: 6px; color: #c8cdd8; selection-background-color: #3b3f55; "
            "  outline: none; padding: 4px; }"
            "QLineEdit { padding: 7px 12px; border: 1px solid #353845; "
            "  border-radius: 8px; background: #252831; color: #dde0e8; "
            "  min-height: 24px; font-size: 12px; }"
            "QLineEdit:focus { border-color: #818cf8; background: #2a2d3a; }"
            "QSpinBox { padding: 6px 10px; border: 1px solid #353845; "
            "  border-radius: 8px; background: #252831; color: #c8cdd8; "
            "  min-height: 24px; font-size: 12px; }"
            "QSpinBox:hover { border-color: #6366f1; }"
            "QSlider::groove:horizontal { height: 5px; background: #2a2d38; border-radius: 3px; }"
            "QSlider::handle:horizontal { width: 18px; height: 18px; margin: -7px 0; "
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, "
            "  stop:0 #818cf8, stop:1 #6366f1); border-radius: 9px; border: 2px solid #1e2028; }"
            "QSlider::handle:horizontal:hover { width: 20px; height: 20px; "
            "  margin: -8px 0; border-radius: 10px; }"
            "QCheckBox { spacing: 10px; color: #b8bfcd; font-size: 12px; }"
            "QCheckBox::indicator { width: 19px; height: 19px; border: 2px solid #454a58; "
            "  border-radius: 5px; background: #252831; }"
            "QCheckBox::indicator:checked { background: #6366f1; border-color: #6366f1; }"
            "QCheckBox::indicator:hover { border-color: #818cf8; }"
            "QLabel { color: #b8bfcd; }"
            "QScrollBar:vertical { width: 8px; background: transparent; margin: 4px 0; }"
            "QScrollBar::handle:vertical { background: #353845; border-radius: 4px; "
            "  min-height: 24px; }"
            "QScrollBar::handle:vertical:hover { background: #4a4f60; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        ));
    } else {
        setStyleSheet(QStringLiteral(
            "QDialog { background: #f3f4f6; }"
            "QTabWidget::pane { border: 1px solid #e0e4ea; border-radius: 10px; "
            "  background: #ffffff; margin-top: -1px; padding: 4px; }"
            "QTabBar::tab { padding: 10px 22px; margin-right: 3px; "
            "  border: none; border-bottom: 2px solid transparent; "
            "  background: transparent; color: #6b7280; font-weight: 600; font-size: 13px; }"
            "QTabBar::tab:selected { color: #4f46e5; "
            "  border-bottom: 2px solid #6366f1; background: rgba(99,102,241,0.05); }"
            "QTabBar::tab:hover:!selected { color: #374151; background: rgba(0,0,0,0.03); }"
            "QGroupBox { font-weight: 600; color: #1f2937; border: 1px solid #e0e4ea; "
            "  border-radius: 10px; margin-top: 14px; padding: 18px 14px 14px 14px; "
            "  background: #fafbfc; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 16px; "
            "  padding: 0 8px; color: #4f46e5; font-size: 12px; }"
            "QPushButton { padding: 7px 18px; border: 1px solid #d1d5db; "
            "  border-radius: 8px; background: #ffffff; color: #374151; "
            "  font-weight: 600; font-size: 12px; min-height: 30px; }"
            "QPushButton:hover { background: #f3f4f6; border-color: #6366f1; color: #1f2937; }"
            "QPushButton:pressed { background: #e5e7eb; }"
            "QPushButton#applyBtn { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, "
            "  stop:0 #6366f1, stop:1 #818cf8); color: #fff; border: none; "
            "  padding: 9px 28px; font-size: 13px; }"
            "QPushButton#applyBtn:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, "
            "  stop:0 #4f46e5, stop:1 #6366f1); }"
            "QPushButton#closeBtn { background: #f3f4f6; border: 1px solid #d1d5db; }"
            "QPushButton#closeBtn:hover { background: #fee2e2; border-color: #ef4444; }"
            "QComboBox { padding: 6px 12px; border: 1px solid #d1d5db; "
            "  border-radius: 8px; background: #ffffff; color: #374151; "
            "  min-height: 26px; font-size: 12px; }"
            "QComboBox:hover { border-color: #6366f1; }"
            "QComboBox::drop-down { border: none; width: 26px; }"
            "QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #d1d5db; "
            "  border-radius: 6px; color: #374151; selection-background-color: #eef2ff; "
            "  outline: none; padding: 4px; }"
            "QLineEdit { padding: 7px 12px; border: 1px solid #d1d5db; "
            "  border-radius: 8px; background: #ffffff; color: #1f2937; "
            "  min-height: 24px; font-size: 12px; }"
            "QLineEdit:focus { border-color: #6366f1; background: #fafbff; }"
            "QSpinBox { padding: 6px 10px; border: 1px solid #d1d5db; "
            "  border-radius: 8px; background: #ffffff; color: #374151; "
            "  min-height: 24px; font-size: 12px; }"
            "QSpinBox:hover { border-color: #6366f1; }"
            "QSlider::groove:horizontal { height: 5px; background: #e5e7eb; border-radius: 3px; }"
            "QSlider::handle:horizontal { width: 18px; height: 18px; margin: -7px 0; "
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, "
            "  stop:0 #818cf8, stop:1 #6366f1); border-radius: 9px; border: 2px solid #ffffff; }"
            "QSlider::handle:horizontal:hover { width: 20px; height: 20px; "
            "  margin: -8px 0; border-radius: 10px; }"
            "QCheckBox { spacing: 10px; color: #374151; font-size: 12px; }"
            "QCheckBox::indicator { width: 19px; height: 19px; border: 2px solid #d1d5db; "
            "  border-radius: 5px; background: #ffffff; }"
            "QCheckBox::indicator:checked { background: #6366f1; border-color: #6366f1; }"
            "QCheckBox::indicator:hover { border-color: #818cf8; }"
            "QLabel { color: #374151; }"
            "QScrollBar:vertical { width: 8px; background: transparent; margin: 4px 0; }"
            "QScrollBar::handle:vertical { background: #d1d5db; border-radius: 4px; "
            "  min-height: 24px; }"
            "QScrollBar::handle:vertical:hover { background: #9ca3af; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        ));
    }

    // Refresh area/status labels to match theme
    refreshAreas();
    if (m_detStatusLabel) checkModelStatus();
    updatePreview();
}

void SettingsPanel::applyStyle() {
    m_config->saveStyle(m_pendingStyle);

    // Active translator was already saved on combo change

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
    m_tabWidget->setTabText(0, tr("General"));
    m_tabWidget->setTabText(1, tr("Translation"));
    m_tabWidget->setTabText(2, tr("Hotkeys"));
    m_tabWidget->setTabText(3, tr("Areas"));

    // Stored labels with their English text keys
    for (auto& pair : m_i18nLabels) {
        if (pair.first == m_modelsInfoLabel) {
            // Special case: append model directory path
            pair.first->setText(tr(pair.second) + m_modelDir);
        } else {
            pair.first->setText(tr(pair.second));
        }
    }

    // Stored group boxes
    for (auto& pair : m_i18nGroups)
        pair.first->setTitle(tr(pair.second));

    // Stored checkboxes
    for (auto& pair : m_i18nCheckBoxes)
        pair.first->setText(tr(pair.second));

    // Stored buttons (fixed-text only)
    for (auto& pair : m_i18nButtons)
        pair.first->setText(tr(pair.second));

    m_resetBtn->setText(tr("Reset Defaults"));
    m_applyBtn->setText(tr("Apply"));
    m_closeBtn->setText(tr("Close"));

    // Translator config group box title: "TranslatorName Configuration"
    if (m_translatorConfigGroup) {
        // Find active translator name
        QString activeName;
        for (auto* combo : {m_onlineCombo, m_llmCombo, m_localCombo}) {
            if (!combo || combo->currentIndex() < 0) continue;
            activeName = combo->currentData().toString();
            if (!activeName.isEmpty()) break;
        }
        if (!activeName.isEmpty())
            m_translatorConfigGroup->setTitle(activeName + " " + tr("Configuration"));
    }

    // Hotkey binding labels — re-translate label portion using binding ID→key mapping
    static const QHash<QString, const char*> hotkeyLabelKeys = {
        {"mode_toggle",     "Toggle Mode"},
        {"area_select",     "Select Area"},
        {"global_hide",     "Show/Hide All"},
        {"long_press",      "Long-Press Translate"},
        {"settings",        "Open Settings"},
        {"snapshot_once",   "Single Snapshot"},
        {"interact_toggle", "Copy Text Mode"},
    };
    for (auto& pair : m_hotkeyLabelIds) {
        QLabel* lbl = pair.first;
        const QString& id = pair.second;
        if (hotkeyLabelKeys.contains(id))
            lbl->setText(tr(hotkeyLabelKeys[id]));
    }

    // Refresh dynamic content that contains translatable strings
    refreshAreas();
    checkModelStatus();
    updatePreview();
}

// ================================================================
//  Prompt Presets
// ================================================================

QVector<PromptPreset> SettingsPanel::loadOrInitPresets() {
    auto presets = m_config->loadPromptPresets();

    // Built-in preset definitions (single source of truth)
    struct BuiltIn { QString id; QString name; QString prompt; };
    QVector<BuiltIn> builtins = {
        {"general", tr("General Translation"),
            "You are a professional translator.\n"
            "Translate the following text from %1 to %2.\n"
            "Keep the translation accurate, natural, and fluent."},

        {"programming", tr("Programming & Tech"),
            "You are a senior software engineer and technical translator.\n"
            "Translate the following text from %1 to %2.\n"
            "Rules:\n"
            "- Keep ALL code, variable names, function names, and API names in English.\n"
            "- Keep technical terms accurate (e.g., 'dependency injection', 'middleware').\n"
            "- Translate comments and documentation naturally.\n"
            "- Prefer industry-standard Chinese translations for well-known terms."},

        {"literature", tr("Literature & Novel"),
            "You are an award-winning literary translator.\n"
            "Translate the following text from %1 to %2.\n"
            "Rules:\n"
            "- Preserve the author's tone, emotion, and stylistic voice.\n"
            "- Pay attention to rhythm, pacing, and imagery.\n"
            "- Use elegant, natural language — not word-for-word.\n"
            "- Cultural references may be adapted for understanding."},

        {"subtitles", tr("Film & Subtitles"),
            "You are a professional subtitler for films and TV shows.\n"
            "Translate the following text from %1 to %2.\n"
            "Rules:\n"
            "- Keep each line short and readable (max 20 characters per line).\n"
            "- Use natural spoken language, not written style.\n"
            "- Convey tone and emotion concisely.\n"
            "- Avoid literal translations that sound unnatural when spoken."},

        {"academic", tr("Academic & Research"),
            "You are a research paper translator.\n"
            "Translate the following text from %1 to %2.\n"
            "Rules:\n"
            "- Use formal, precise academic language.\n"
            "- Keep discipline-specific terminology accurate.\n"
            "- Maintain the logical structure and argument flow.\n"
            "- Do NOT simplify or summarize — preserve full meaning."},

        {"game", tr("Game Localization"),
            "You are a video game localization specialist.\n"
            "Translate the following text from %1 to %2.\n"
            "Rules:\n"
            "- Use creative, immersive language fitting the game's world.\n"
            "- UI text: short, punchy, consistent (e.g., 'Settings' → '设置').\n"
            "- Dialogue: natural, character-appropriate voice.\n"
            "- Lore/descriptions: vivid and atmospheric.\n"
            "- Keep placeholder formats like {0}, %s, <color=...> unchanged."},

        {"business", tr("Business & Legal"),
            "You are a corporate communications translator.\n"
            "Translate the following text from %1 to %2.\n"
            "Rules:\n"
            "- Use formal business language appropriate for the context.\n"
            "- Legal terms must be precise and standard.\n"
            "- Maintain professional tone throughout.\n"
            "- Numbers, dates, and currency formats should follow target locale conventions."},

        {"unreal", tr("Unreal Engine Dev"),
            "You are an Unreal Engine 5 technical translator.\n"
            "Translate the following text from %1 to %2.\n"
            "Rules:\n"
            "- Keep ALL class names, function names, variable names, macros, and API symbols in English.\n"
            "- Translate UI labels, tooltips, menu items, and editor text naturally.\n"
            "- UE-specific glossary:\n"
            "  Actor → Actor, Pawn → Pawn, Character → 角色, Controller → 控制器,\n"
            "  GameMode → 游戏模式, GameState → 游戏状态, PlayerState → 玩家状态,\n"
            "  Viewport/VP → 视口, Blueprint → 蓝图, Widget → 控件,\n"
            "  Level → 关卡, World → 世界, Scene → 场景, Asset → 资产,\n"
            "  Material → 材质, Texture → 贴图, Mesh → 网格体, Skeleton → 骨架,\n"
            "  Animation → 动画, Montage → 蒙太奇, Blend Space → 混合空间,\n"
            "  Behavior Tree → 行为树, Blackboard → 黑板, EQS → 环境查询系统,\n"
            "  Niagara → Niagara, Cascade → Cascade, Particle → 粒子,\n"
            "  Collision → 碰撞, Trigger → 触发器, Volume → 体积,\n"
            "  Lightmass → Lightmass, LOD → LOD, Nanite → Nanite, Lumen → Lumen,\n"
            "  Sequencer → Sequencer, Take → Take, Cinematic → 过场动画,\n"
            "  Gameplay Ability System/GAS → GAS 技能系统,\n"
            "  Replication → 复制, Network → 网络, RPC → RPC,\n"
            "- For editor UI: prefer short, standard translations used in Epic's official Chinese docs.\n"
            "- If a term has no standard translation, keep it in English."},
    };

    // Build set of existing built-in IDs
    QSet<QString> existingIds;
    for (const auto& p : presets) {
        if (p.isBuiltIn)
            existingIds.insert(p.id);
    }

    // Add any missing built-in presets (supports adding new presets in future versions)
    bool added = false;
    for (const auto& b : builtins) {
        if (!existingIds.contains(b.id)) {
            PromptPreset p;
            p.id        = b.id;
            p.name      = b.name;
            p.prompt    = b.prompt;
            p.isBuiltIn = true;
            presets.append(p);
            added = true;
        }
    }

    if (added)
        m_config->savePromptPresets(presets);

    return presets;
}

void SettingsPanel::applyActivePrompt() {
    QString id = m_config->activePromptId();
    auto presets = m_config->loadPromptPresets();
    for (const auto& p : presets) {
        if (p.id == id) {
            // Apply to active translator
            QString activeName = m_config->activeTranslator();
            for (auto* t : m_translators) {
                if (t->name() == activeName) {
                    t->setConfig("systemPrompt", p.prompt);
                    m_config->setTranslatorConfig(t->name(), "systemPrompt", p.prompt);
                    break;
                }
            }
            return;
        }
    }
}

void SettingsPanel::openPromptManager() {
    auto presets = m_config->loadPromptPresets();
    QString activeId = m_config->activePromptId();

    PromptManagerDialog dlg(presets, activeId, this);
    if (dlg.exec() == QDialog::Accepted) {
        auto updated = dlg.presets();
        m_config->savePromptPresets(updated);
        m_config->setActivePromptId(dlg.activeId());

        // Refresh combo
        m_promptCombo->blockSignals(true);
        m_promptCombo->clear();
        for (const auto& p : updated) {
            m_promptCombo->addItem(p.name, p.id);
            if (p.id == dlg.activeId())
                m_promptCombo->setCurrentIndex(m_promptCombo->count() - 1);
        }
        m_promptCombo->blockSignals(false);

        applyActivePrompt();
    }
}

// ================================================================
//  Models tab — PaddleOCR ONNX model download & status
// ================================================================

QWidget* SettingsPanel::createModelsTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    m_modelDir = QCoreApplication::applicationDirPath() + "/models";
    QDir().mkpath(m_modelDir);  // always use exe dir, create if missing

    m_modelsInfoLabel = new QLabel(tr("PaddleOCR models enable offline, high-accuracy "
                                     "text detection and recognition.\n"
                                     "Model directory: ") + m_modelDir);
    m_modelsInfoLabel->setWordWrap(true);
    m_i18nLabels.append({m_modelsInfoLabel, "PaddleOCR models enable offline, high-accuracy text detection and recognition.\nModel directory: "});
    layout->addWidget(m_modelsInfoLabel);

    // Status group
    m_modelsStatusGroup = new QGroupBox(tr("Model Status"));
    m_i18nGroups.append({m_modelsStatusGroup, "Model Status"});
    auto* statusForm = new QFormLayout(m_modelsStatusGroup);

    m_detStatusLabel  = new QLabel(tr("Checking..."));
    m_recStatusLabel  = new QLabel(tr("Checking..."));
    m_keysStatusLabel = new QLabel(tr("Checking..."));
    m_modelsDetFormLabel  = new QLabel(tr("Detection model:"));
    m_modelsRecFormLabel  = new QLabel(tr("Recognition model:"));
    m_modelsKeysFormLabel = new QLabel(tr("Character dictionary:"));
    m_i18nLabels.append({m_modelsDetFormLabel,  "Detection model:"});
    m_i18nLabels.append({m_modelsRecFormLabel,  "Recognition model:"});
    m_i18nLabels.append({m_modelsKeysFormLabel, "Character dictionary:"});
    statusForm->addRow(m_modelsDetFormLabel,  m_detStatusLabel);
    statusForm->addRow(m_modelsRecFormLabel,  m_recStatusLabel);
    statusForm->addRow(m_modelsKeysFormLabel, m_keysStatusLabel);
    layout->addWidget(m_modelsStatusGroup);

    // Download button
    m_downloadBtn = new QPushButton(tr("Download Models (~13 MB)"));
    m_downloadBtn->setMinimumHeight(36);
    connect(m_downloadBtn, &QPushButton::clicked, this, &SettingsPanel::startModelDownload);
    layout->addWidget(m_downloadBtn);

    // Progress
    m_dlProgressLabel = new QLabel;
    m_dlProgressLabel->setWordWrap(true);
    layout->addWidget(m_dlProgressLabel);

    layout->addStretch();
    checkModelStatus();
    return page;
}

void SettingsPanel::checkModelStatus() {
    auto setLabel = [](QLabel* lbl, bool ok, const QString& name) {
        if (ok) {
            lbl->setText(QString::fromUtf8("\xe2\x9c\x93 ") + name + " OK");
            lbl->setStyleSheet("color: #10b981; font-weight: 600;");
        } else {
            lbl->setText(QString::fromUtf8("\xe2\x9c\x97 ") + name + " " +
                         QObject::tr("not found"));
            lbl->setStyleSheet("color: #f87171; font-weight: 600;");
        }
    };

    QDir dir(m_modelDir);
    setLabel(m_detStatusLabel,
             QFileInfo::exists(dir.filePath("ch_PP-OCRv4_det_infer.onnx")),
             "ch_PP-OCRv4_det_infer.onnx");
    setLabel(m_recStatusLabel,
             QFileInfo::exists(dir.filePath("ch_PP-OCRv4_rec_infer.onnx")),
             "ch_PP-OCRv4_rec_infer.onnx");
    setLabel(m_keysStatusLabel,
             QFileInfo::exists(dir.filePath("ppocr_keys_v1.txt")),
             "ppocr_keys_v1.txt");

    bool allOk = QFileInfo::exists(dir.filePath("ch_PP-OCRv4_det_infer.onnx"))
              && QFileInfo::exists(dir.filePath("ch_PP-OCRv4_rec_infer.onnx"))
              && QFileInfo::exists(dir.filePath("ppocr_keys_v1.txt"));
    m_downloadBtn->setEnabled(!allOk);
    if (allOk)
        m_downloadBtn->setText(tr("All Models Ready"));
}

void SettingsPanel::startModelDownload() {
    m_downloadBtn->setEnabled(false);
    m_dlProgressLabel->setText(tr("Downloading..."));

    QDir().mkpath(m_modelDir);

    struct ModelFile { QString url; QString filename; };
    QVector<ModelFile> files = {
        {"https://www.modelscope.cn/api/v1/models/RapidAI/RapidOCR/repo?"
         "Revision=master&FilePath=ch_PP-OCRv4_det_infer.onnx",
         "ch_PP-OCRv4_det_infer.onnx"},
        {"https://www.modelscope.cn/api/v1/models/RapidAI/RapidOCR/repo?"
         "Revision=master&FilePath=ch_PP-OCRv4_rec_infer.onnx",
         "ch_PP-OCRv4_rec_infer.onnx"},
        {"https://www.modelscope.cn/api/v1/models/RapidAI/RapidOCR/repo?"
         "Revision=master&FilePath=ppocr_keys_v1.txt",
         "ppocr_keys_v1.txt"},
    };

    auto* nam = new QNetworkAccessManager(this);
    auto pending = std::make_shared<int>(files.size());

    for (const auto& f : files) {
        auto* reply = nam->get(QNetworkRequest(QUrl(f.url)));
        QString destPath = m_modelDir + "/" + f.filename;

        connect(reply, &QNetworkReply::finished, this, [=]() {
            reply->deleteLater();
            QString result;
            if (reply->error() == QNetworkReply::NoError) {
                QFile out(destPath);
                if (out.open(QIODevice::WriteOnly)) {
                    QByteArray data = reply->readAll();
                    out.write(data);
                    out.close();
                    result = QString("%1: OK (%2 KB)").arg(f.filename).arg(data.size()/1024);
                }
            } else {
                result = QString("%1: %2").arg(f.filename, reply->errorString());
                qWarning() << "Model download failed:" << result;
            }
            m_dlProgressLabel->setText(m_dlProgressLabel->text() + "\n" + result);
            if (--(*pending) == 0) {
                nam->deleteLater();
                checkModelStatus();
                if (!QFileInfo::exists(m_modelDir + "/ch_PP-OCRv4_det_infer.onnx"))
                    m_dlProgressLabel->setText(m_dlProgressLabel->text() + "\n\n" +
                        tr("Auto-download failed. Please download manually:\n"
                           "https://www.modelscope.cn/models/RapidAI/RapidOCR/files\n"
                           "Or: https://github.com/RapidAI/RapidOCR/releases\n"
                           "Place 3 files in: ") + m_modelDir);
                else
                    m_dlProgressLabel->setText(m_dlProgressLabel->text() + "\n" +
                        tr("Done. Restart to use PaddleOCR."));
            }
        });
    }
}
