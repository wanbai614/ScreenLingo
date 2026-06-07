#include "Config.h"

Config::Config(const QString& org, const QString& app)
    : m_settings(org, app) {}

StyleConfig Config::loadStyle() {
    StyleConfig s;
    m_settings.beginGroup("Style");
    s.textColor       = m_settings.value("textColor", s.textColor).value<QColor>();
    s.backgroundColor = m_settings.value("backgroundColor", s.backgroundColor).value<QColor>();
    s.backgroundAlpha = m_settings.value("backgroundAlpha", s.backgroundAlpha).toInt();
    s.borderRadius    = m_settings.value("borderRadius", s.borderRadius).toInt();
    s.borderColor     = m_settings.value("borderColor", s.borderColor).value<QColor>();
    s.borderWidth     = m_settings.value("borderWidth", s.borderWidth).toInt();
    QString fontFamily = m_settings.value("fontFamily", s.font.family()).toString();
    int fontSize       = m_settings.value("fontSize", s.font.pointSize()).toInt();
    s.font = QFont(fontFamily, fontSize);
    m_settings.endGroup();
    return s;
}

void Config::saveStyle(const StyleConfig& s) {
    m_settings.beginGroup("Style");
    m_settings.setValue("textColor", s.textColor);
    m_settings.setValue("backgroundColor", s.backgroundColor);
    m_settings.setValue("backgroundAlpha", s.backgroundAlpha);
    m_settings.setValue("borderRadius", s.borderRadius);
    m_settings.setValue("borderColor", s.borderColor);
    m_settings.setValue("borderWidth", s.borderWidth);
    m_settings.setValue("fontFamily", s.font.family());
    m_settings.setValue("fontSize", s.font.pointSize());
    m_settings.endGroup();
}

QString Config::sourceLang() const {
    return m_settings.value("Language/source", "auto").toString();
}
void Config::setSourceLang(const QString& lang) {
    m_settings.setValue("Language/source", lang);
}

QString Config::targetLang() const {
    return m_settings.value("Language/target", "zh").toString();
}
void Config::setTargetLang(const QString& lang) {
    m_settings.setValue("Language/target", lang);
}

Mode Config::lastMode() const {
    return static_cast<Mode>(m_settings.value("App/mode", static_cast<int>(Mode::Snapshot)).toInt());
}
void Config::setLastMode(Mode mode) {
    m_settings.setValue("App/mode", static_cast<int>(mode));
}

QString Config::theme() const {
    return m_settings.value("App/theme", "dark").toString();
}
void Config::setTheme(const QString& theme) {
    m_settings.setValue("App/theme", theme);
}

QString Config::activeTranslator() const {
    return m_settings.value("App/activeTranslator", "").toString();
}
void Config::setActiveTranslator(const QString& name) {
    m_settings.setValue("App/activeTranslator", name);
}

QString Config::ocrEngine() const {
    return m_settings.value("App/ocrEngine", "windows").toString();
}
void Config::setOCREngine(const QString& name) {
    m_settings.setValue("App/ocrEngine", name);
}

bool Config::vlmSnapshot() const {
    return m_settings.value("App/vlmSnapshot", false).toBool();
}
void Config::setVlmSnapshot(bool enabled) {
    m_settings.setValue("App/vlmSnapshot", enabled);
}

bool Config::uiTranslateMode() const {
    return m_settings.value("App/uiTranslateMode", false).toBool();
}
void Config::setUITranslateMode(bool enabled) {
    m_settings.setValue("App/uiTranslateMode", enabled);
}

QVector<SelectionArea> Config::loadAreas() {
    QVector<SelectionArea> areas;
    int count = m_settings.beginReadArray("Areas");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        SelectionArea a;
        a.id          = m_settings.value("id").toInt();
        a.screenIndex = m_settings.value("screenIndex").toInt();
        a.geometry    = m_settings.value("geometry").toRect();
        a.enabled     = m_settings.value("enabled", true).toBool();
        areas.append(a);
    }
    m_settings.endArray();
    return areas;
}

void Config::saveAreas(const QVector<SelectionArea>& areas) {
    m_settings.beginWriteArray("Areas", areas.size());
    for (int i = 0; i < areas.size(); ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue("id", areas[i].id);
        m_settings.setValue("screenIndex", areas[i].screenIndex);
        m_settings.setValue("geometry", areas[i].geometry);
        m_settings.setValue("enabled", areas[i].enabled);
    }
    m_settings.endArray();
}

QVector<HotkeyBinding> Config::loadHotkeys(const QVector<HotkeyBinding>& defaults) {
    QVector<HotkeyBinding> result;
    m_settings.beginGroup("Hotkeys");
    for (const auto& def : defaults) {
        HotkeyBinding b = def;
        b.currentKeys = m_settings.value(def.id, def.defaultKeys).toString();
        result.append(b);
    }
    m_settings.endGroup();
    return result;
}

void Config::saveHotkeys(const QVector<HotkeyBinding>& bindings) {
    m_settings.beginGroup("Hotkeys");
    for (const auto& b : bindings) {
        m_settings.setValue(b.id, b.currentKeys);
    }
    m_settings.endGroup();
}

QVector<PromptPreset> Config::loadPromptPresets() const {
    QVector<PromptPreset> presets;
    int count = m_settings.beginReadArray("PromptPresets");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        PromptPreset p;
        p.id        = m_settings.value("id").toString();
        p.name      = m_settings.value("name").toString();
        p.prompt    = m_settings.value("prompt").toString();
        p.isBuiltIn = m_settings.value("isBuiltIn", false).toBool();
        presets.append(p);
    }
    m_settings.endArray();
    return presets;
}

void Config::savePromptPresets(const QVector<PromptPreset>& presets) {
    m_settings.beginWriteArray("PromptPresets", presets.size());
    for (int i = 0; i < presets.size(); ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue("id",        presets[i].id);
        m_settings.setValue("name",      presets[i].name);
        m_settings.setValue("prompt",    presets[i].prompt);
        m_settings.setValue("isBuiltIn", presets[i].isBuiltIn);
    }
    m_settings.endArray();
}

QString Config::activePromptId() const {
    return m_settings.value("App/activePromptId", "general").toString();
}
void Config::setActivePromptId(const QString& id) {
    m_settings.setValue("App/activePromptId", id);
}

QString Config::translatorConfig(const QString& pluginName, const QString& key) const {
    return m_settings.value(QString("Translators/%1/%2").arg(pluginName, key)).toString();
}
void Config::setTranslatorConfig(const QString& pluginName, const QString& key,
                                 const QString& value) {
    m_settings.setValue(QString("Translators/%1/%2").arg(pluginName, key), value);
}
