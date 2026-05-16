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
                  QWidget* parent = nullptr);

signals:
    void styleChanged(const StyleConfig& style);
    void languageChangeRequested(const QString& lang);

public slots:
    void retranslateUi();

private:
    QWidget* createAppearanceTab();
    QWidget* createTranslationTab();
    QWidget* createHotkeysTab();
    QWidget* createAreasTab();

    void applyStyle();
    void loadStyle();
    void updatePreview();

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

    QComboBox*   m_serviceCombo = nullptr;
    QVBoxLayout* m_translatorConfigLayout = nullptr;

    QLabel*      m_previewLabel = nullptr;
    QComboBox*   m_langCombo    = nullptr;
    QPushButton* m_resetBtn     = nullptr;
    QPushButton* m_applyBtn     = nullptr;
    QPushButton* m_closeBtn     = nullptr;
};
