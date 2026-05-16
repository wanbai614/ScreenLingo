#include "LanguageManager.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtCore/QDir>
#include <QtWidgets/QWidget>

LanguageManager* LanguageManager::s_instance = nullptr;

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent) {}

LanguageManager* LanguageManager::instance() {
    if (!s_instance) {
        s_instance = new LanguageManager();
    }
    return s_instance;
}

void LanguageManager::initialize(const QString& lang) {
    if (lang.isEmpty()) {
        QString sysLang = QLocale::system().name();  // e.g. "zh_CN"
        switchLanguage(sysLang.startsWith("zh") ? "zh_CN" : "en");
    } else {
        switchLanguage(lang);
    }
}

QVector<QPair<QString, QString>> LanguageManager::availableLanguages() const {
    return {
        {"en",    "English"},
        {"zh_CN", "简体中文"},
    };
}

void LanguageManager::switchLanguage(const QString& lang) {
    if (m_currentLang == lang) return;
    m_currentLang = lang;

    QCoreApplication::removeTranslator(&m_translator);

    if (lang == "zh_CN") {
        QString path = QCoreApplication::applicationDirPath()
                       + "/translations/screenlingo_zh_CN.qm";
        if (m_translator.load(path)) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    // "en" uses no translator (source strings)

    emit languageChanged(lang);
}
