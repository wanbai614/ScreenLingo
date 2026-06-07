#pragma once

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QVector>
#include "core/translate/ITranslator.h"

class DeepSeekTranslator : public ITranslator {
    Q_OBJECT

public:
    explicit DeepSeekTranslator(QObject* parent = nullptr);

    QString name() const override { return "DeepSeek"; }
    QString category() const override { return "llm"; }
    bool isAvailable() const override { return !m_apiKey.isEmpty(); }

    void translate(const TranslateRequest& req) override;
    void cancelAll() override;

    QVector<TranslatorConfigField> configFields() const override;
    void setConfig(const QString& key, const QString& value) override;
    QString getConfig(const QString& key) const override;

private:
    QNetworkAccessManager* m_nam;
    QString m_apiKey;
    QString m_baseUrl    = "https://api.deepseek.com/v1";
    QString m_model      = "deepseek-chat";
    QString m_systemPrompt =
        "You are a translator. Translate the given text from %1 to %2. "
        "Each line is a separate text to translate. "
        "Keep the same number of lines in your output."
        "Output ONLY the translation. No explanations, no notes, no thinking.";
    QVector<QNetworkReply*> m_pendingRequests;
};
