#pragma once

#include <QtCore/QSettings>
#include <QtCore/QString>
#include "Types.h"

class Config {
public:
    explicit Config(const QString& org, const QString& app);

    // Style
    StyleConfig loadStyle();
    void saveStyle(const StyleConfig& style);

    // Language
    QString sourceLang() const;
    void setSourceLang(const QString& lang);
    QString targetLang() const;
    void setTargetLang(const QString& lang);

    // Mode
    Mode lastMode() const;
    void setLastMode(Mode mode);

    // Active translator
    QString activeTranslator() const;
    void setActiveTranslator(const QString& name);

    // Selected areas
    QVector<SelectionArea> loadAreas();
    void saveAreas(const QVector<SelectionArea>& areas);

    // Hotkeys
    QVector<HotkeyBinding> loadHotkeys(const QVector<HotkeyBinding>& defaults);
    void saveHotkeys(const QVector<HotkeyBinding>& bindings);

    // Per-translator config (non-secret: endpoints, models, etc.)
    QString translatorConfig(const QString& pluginName, const QString& key) const;
    void setTranslatorConfig(const QString& pluginName, const QString& key,
                             const QString& value);

private:
    mutable QSettings m_settings;
};
