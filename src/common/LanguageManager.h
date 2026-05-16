#pragma once

#include <QtCore/QObject>
#include <QtCore/QTranslator>
#include <QtCore/QString>
#include <QtCore/QVector>

class LanguageManager : public QObject {
    Q_OBJECT

public:
    static LanguageManager* instance();

    void initialize(const QString& lang = QString{});

    QString currentLanguage() const { return m_currentLang; }
    QVector<QPair<QString, QString>> availableLanguages() const;

    void switchLanguage(const QString& lang);

signals:
    void languageChanged(const QString& lang);

private:
    explicit LanguageManager(QObject* parent = nullptr);
    void loadTranslation(const QString& lang);

    QTranslator m_translator;
    QString     m_currentLang;
    static LanguageManager* s_instance;
};
