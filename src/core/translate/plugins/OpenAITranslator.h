#pragma once

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QVector>
#include "core/translate/ITranslator.h"

class OpenAITranslator : public ITranslator {
    Q_OBJECT

public:
    explicit OpenAITranslator(QObject* parent = nullptr);

    QString name() const override { return "OpenAI"; }
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
    QString m_baseUrl    = "https://api.openai.com/v1";
    QString m_model      = "gpt-4o";
    QString m_systemPrompt =
        "You are a professional translator. Translate the following text "
        "naturally while preserving tone and meaning. Output only the "
        "translation, nothing else.";
    QVector<QNetworkReply*> m_pendingRequests;
};
