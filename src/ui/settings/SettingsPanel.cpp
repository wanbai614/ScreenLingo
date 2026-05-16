#include "SettingsPanel.h"
#include "core/translate/ITranslator.h"
#include "engine/hotkey/HotkeyManager.h"
#include "common/Config.h"
#include "app/SignalBus.h"
#include <QtGui/QFontDatabase>
#include <QtWidgets/QMessageBox>

SettingsPanel::SettingsPanel(const QVector<ITranslator*>& translators,
                               HotkeyManager* hotkeyMgr,
                               Config* config,
                               SignalBus* bus,
                               QWidget* parent)
    : QDialog(parent), m_bus(bus), m_config(config),
      m_hotkeyMgr(hotkeyMgr), m_translators(translators) {

    setWindowTitle("ScreenLingo Settings");
    setMinimumSize(520, 440);

    auto* tabWidget = new QTabWidget;

    tabWidget->addTab(createAppearanceTab(),  "Appearance");
    tabWidget->addTab(createTranslationTab(), "Translation");
    tabWidget->addTab(createHotkeysTab(),     "Hotkeys");
    tabWidget->addTab(createAreasTab(),       "Areas");

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabWidget);

    auto* btnLayout = new QHBoxLayout;
    auto* resetBtn  = new QPushButton("Reset Defaults");
    auto* applyBtn  = new QPushButton("Apply");
    auto* closeBtn  = new QPushButton("Close");
    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    connect(applyBtn,  &QPushButton::clicked, this, &SettingsPanel::applyStyle);
    connect(closeBtn,  &QPushButton::clicked, this, &QDialog::close);
    connect(resetBtn,  &QPushButton::clicked, this, [this]() {
        m_pendingStyle = StyleConfig{};
        loadStyle();
        updatePreview();
    });

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
    form->addRow("Text Color:", m_textColorBtn);

    m_fontSizeSpin = new QSpinBox;
    m_fontSizeSpin->setRange(8, 48);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.font.setPointSize(v);
        updatePreview();
    });
    form->addRow("Font Size:", m_fontSizeSpin);

    m_fontCombo = new QComboBox;
    QFontDatabase fontDb;
    m_fontCombo->addItems(fontDb.families());
    connect(m_fontCombo, &QComboBox::currentTextChanged, this, [this](const QString& f) {
        m_pendingStyle.font.setFamily(f);
        updatePreview();
    });
    form->addRow("Font:", m_fontCombo);

    m_bgColorBtn = new QPushButton;
    m_bgColorBtn->setFixedSize(40, 24);
    connect(m_bgColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_pendingStyle.backgroundColor, this,
                                          "Choose", QColorDialog::ShowAlphaChannel);
        if (c.isValid()) {
            m_pendingStyle.backgroundColor = c;
            m_bgColorBtn->setStyleSheet(
                QString("background-color: %1; border: 1px solid #666;").arg(c.name()));
            updatePreview();
        }
    });
    form->addRow("Background:", m_bgColorBtn);

    m_alphaSlider = new QSlider(Qt::Horizontal);
    m_alphaSlider->setRange(0, 100);
    connect(m_alphaSlider, &QSlider::valueChanged, this, [this](int v) {
        m_pendingStyle.backgroundAlpha = v;
        updatePreview();
    });
    form->addRow("Opacity:", m_alphaSlider);

    m_borderRadiusSpin = new QSpinBox;
    m_borderRadiusSpin->setRange(0, 20);
    connect(m_borderRadiusSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.borderRadius = v;
        updatePreview();
    });
    form->addRow("Border Radius:", m_borderRadiusSpin);

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
    form->addRow("Border Color:", m_borderColorBtn);

    m_borderWidthSpin = new QSpinBox;
    m_borderWidthSpin->setRange(0, 5);
    connect(m_borderWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_pendingStyle.borderWidth = v;
        updatePreview();
    });
    form->addRow("Border Width:", m_borderWidthSpin);

    auto* previewGroup = new QGroupBox("Preview");
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel("Hello World!");
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
    layout->addWidget(new QLabel("Active Service:"));
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

        auto* group = new QGroupBox(translator->name() + " Configuration");
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

    if (m_translators.size() > 0)
        m_serviceCombo->setCurrentIndex(0);

    layout->addStretch();
    return page;
}

QWidget* SettingsPanel::createHotkeysTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel("Hotkey bindings:"));

    for (const auto& binding : m_hotkeyMgr->bindings()) {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(binding.label));
        auto* keyBtn = new QPushButton(binding.currentKeys);
        keyBtn->setMinimumWidth(120);

        connect(keyBtn, &QPushButton::clicked, this, [this, binding, keyBtn]() {
            keyBtn->setText("Press keys...");
            keyBtn->setEnabled(false);
            QMessageBox::information(this, "Rebind",
                QString("Key capture for '%1'.\nCurrent: %2\n\n"
                        "In production: press the new key combination.")
                .arg(binding.label, binding.currentKeys));
            keyBtn->setText(binding.currentKeys);
            keyBtn->setEnabled(true);
        });

        row->addWidget(keyBtn);
        row->addStretch();
        layout->addLayout(row);
    }

    auto* resetBtn = new QPushButton("Reset All to Defaults");
    layout->addWidget(resetBtn);
    layout->addStretch();
    return page;
}

QWidget* SettingsPanel::createAreasTab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel("Translation Areas:"));
    auto* areaList = new QLabel("No areas configured.\nUse Ctrl+Shift+A "
                                 "or the tray menu to select areas.");
    areaList->setWordWrap(true);
    layout->addWidget(areaList);
    layout->addStretch();
    return page;
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

    emit styleChanged(m_pendingStyle);
    if (m_bus) m_bus->styleChanged(m_pendingStyle);
}
