#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtCore/QVector>
#include "common/Types.h"

class ITranslator;
class HotkeyManager;
class Config;
class SignalBus;

class SettingsPanel : public QDialog {
    Q_OBJECT

public:
    SettingsPanel(const QVector<ITranslator*>& translators,
                  HotkeyManager* hotkeyMgr,
                  Config* config,
                  SignalBus* bus,
                  const QVector<SelectionArea>* areas,
                  QWidget* parent = nullptr);

    void refreshAreas();

signals:
    void styleChanged(const StyleConfig& style);
    void languageChangeRequested(const QString& lang);
    void areaSelectRequested();
    void areaCleared();
    void areaEnabledChanged(int id, bool enabled);
    void translatorChangeRequested(const QString& name);

public slots:
    void retranslateUi();

private:
    QWidget* createGeneralTab();
    QWidget* createTranslationTab();
    QWidget* createHotkeysTab();
    QWidget* createAreasTab();
    QWidget* createModelsTab();

    void checkModelStatus();
    void startModelDownload();

    void applyStyle();
    void loadStyle();
    void updatePreview();
    void applySettingsTheme();
    void openPromptManager();

    // Prompt presets
    QVector<PromptPreset> loadOrInitPresets();
    void applyActivePrompt();

    SignalBus*                   m_bus;
    Config*                      m_config;
    HotkeyManager*               m_hotkeyMgr;
    QVector<ITranslator*>        m_translators;
    StyleConfig                  m_pendingStyle;

    QTabWidget*   m_tabWidget   = nullptr;
    QPushButton* m_textColorBtn    = nullptr;
    QSpinBox*    m_fontSizeSpin    = nullptr;
    QComboBox*   m_fontCombo       = nullptr;
    QPushButton* m_bgColorBtn      = nullptr;
    QSlider*     m_alphaSlider     = nullptr;
    QSpinBox*    m_borderRadiusSpin = nullptr;
    QPushButton* m_borderColorBtn  = nullptr;
    QSpinBox*    m_borderWidthSpin = nullptr;

    QComboBox*   m_onlineCombo  = nullptr;
    QComboBox*   m_llmCombo     = nullptr;
    QComboBox*   m_localCombo   = nullptr;
    QCheckBox*   m_vlmCb        = nullptr;
    QCheckBox*   m_uiModeCb     = nullptr;
    QComboBox*   m_srcLangCombo = nullptr;
    QComboBox*   m_tgtLangCombo = nullptr;
    QComboBox*   m_promptCombo  = nullptr;
    QPushButton* m_promptMgrBtn = nullptr;
    QComboBox*   m_ocrCombo     = nullptr;
    QVBoxLayout* m_translatorConfigLayout = nullptr;
    QGroupBox*   m_translatorConfigGroup  = nullptr; // current config group for retranslation

    QLabel*      m_previewLabel = nullptr;
    QComboBox*   m_langCombo    = nullptr;
    QComboBox*   m_themeCombo   = nullptr;
    QPushButton* m_resetBtn     = nullptr;
    QPushButton* m_applyBtn     = nullptr;
    QPushButton* m_closeBtn     = nullptr;

    const QVector<SelectionArea>* m_areasPtr = nullptr;
    QLabel*     m_areaInfoLabel  = nullptr;
    QCheckBox*  m_areaEnableCb   = nullptr;
    QPushButton* m_areaReselectBtn = nullptr;
    QPushButton* m_areaClearBtn    = nullptr;
    QLabel*     m_areasTitleLabel  = nullptr;

    // Model download tab
    QLabel*      m_detStatusLabel   = nullptr;
    QLabel*      m_recStatusLabel   = nullptr;
    QLabel*      m_keysStatusLabel  = nullptr;
    QPushButton* m_downloadBtn      = nullptr;
    QLabel*      m_dlProgressLabel  = nullptr;
    QLabel*      m_modelsInfoLabel  = nullptr;
    QGroupBox*   m_modelsStatusGroup = nullptr;
    QLabel*      m_modelsDetFormLabel  = nullptr;
    QLabel*      m_modelsRecFormLabel  = nullptr;
    QLabel*      m_modelsKeysFormLabel = nullptr;
    QString      m_modelDir;

    // Hotkeys tab
    QLabel*      m_hotkeysTitleLabel = nullptr;
    QPushButton* m_hotkeysResetBtn  = nullptr;
    QVector<QPair<QLabel*, QString>> m_hotkeyLabelIds; // label + bindingId

    // Pointers for retranslateUi
    QGroupBox*   m_generalGroup   = nullptr;
    QLabel*      m_langLabel      = nullptr;
    QLabel*      m_ocrLabel       = nullptr;
    QVector<QPair<QLabel*, const char*>>     m_i18nLabels;
    QVector<QPair<QGroupBox*, const char*>>  m_i18nGroups;
    QVector<QPair<QCheckBox*, const char*>>  m_i18nCheckBoxes;
    QVector<QPair<QPushButton*, const char*>> m_i18nButtons;
};
