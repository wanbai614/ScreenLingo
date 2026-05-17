#pragma once

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QVector>
#include "core/translate/ITranslator.h"

class OllamaTranslator : public ITranslator {
    Q_OBJECT

public:
    explicit OllamaTranslator(QObject* parent = nullptr);

    QString name() const override { return "Ollama"; }
    QString category() const override { return "llm"; }
    bool isAvailable() const override { return true; }  // local, always available

    void translate(const TranslateRequest& req) override;
    void cancelAll() override;

    QVector<TranslatorConfigField> configFields() const override;
    void setConfig(const QString& key, const QString& value) override;
    QString getConfig(const QString& key) const override;

private:
    QNetworkAccessManager* m_nam;
    QString m_baseUrl = "http://localhost:11434/v1";
    QString m_model   = "llama3";
    QVector<QNetworkReply*> m_pendingRequests;
};
