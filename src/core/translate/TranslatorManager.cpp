#include "TranslatorManager.h"

TranslatorManager::TranslatorManager(QObject* parent)
    : QObject(parent) {}

void TranslatorManager::registerPlugin(std::unique_ptr<ITranslator> plugin) {
    if (!plugin) return;
    connect(plugin.get(), &ITranslator::translationReady,
            this, &TranslatorManager::onPluginTranslationReady);
    connect(plugin.get(), &ITranslator::translationError,
            this, &TranslatorManager::onPluginTranslationError);
    if (!m_active) m_active = plugin.get();
    m_plugins.push_back(std::move(plugin));
}

ITranslator* TranslatorManager::active() const {
    return m_active;
}

QVector<ITranslator*> TranslatorManager::plugins() const {
    QVector<ITranslator*> result;
    for (const auto& p : m_plugins)
        result.append(p.get());
    return result;
}

ITranslator* TranslatorManager::findByName(const QString& name) const {
    for (const auto& p : m_plugins) {
        if (p->name() == name) return p.get();
    }
    return nullptr;
}

void TranslatorManager::setActive(const QString& name) {
    auto* plugin = findByName(name);
    if (plugin && plugin != m_active) {
        m_active = plugin;
        emit activeChanged(name);
    }
}

void TranslatorManager::translate(const QString& text, const QString& sourceLang,
                                   const QString& targetLang) {
    if (!m_active) {
        emit translationError(tr("No translation service configured"));
        return;
    }
    TranslateRequest req{text, sourceLang, targetLang};
    m_active->translate(req);
}

void TranslatorManager::cancelAll() {
    for (const auto& p : m_plugins)
        p->cancelAll();
}

void TranslatorManager::onPluginTranslationReady(const QString& original,
                                                  const QString& translated) {
    emit translationReady(original, translated);
}

void TranslatorManager::onPluginTranslationError(const QString& error) {
    emit translationError(error);
}
