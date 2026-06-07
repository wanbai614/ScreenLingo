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
    void translateWithImage(const QImage& image, const QString& sourceLang,
                            const QString& targetLang) override;
    void cancelAll() override;

    QVector<TranslatorConfigField> configFields() const override;
    void setConfig(const QString& key, const QString& value) override;
    QString getConfig(const QString& key) const override;

private:
    void processQueue();

    QNetworkAccessManager* m_nam;
    QString m_baseUrl = "http://localhost:11434";
    QString m_model   = "llama3";
    QString m_systemPrompt =
        "You are a translator. Translate the given text from %1 to %2. "
        "Each line is a separate text to translate. "
        "Keep the same number of lines in your output."
        "Output ONLY the translation. No explanations, no notes, no thinking.";
    QVector<QNetworkReply*> m_pendingRequests;

    struct QueuedReq {
        QString text; QString srcLang; QString tgtLang;
        QImage image; bool isVlm = false;
        bool batchMode = false;
    };
    QVector<QueuedReq> m_queue;
    int m_activeRequests = 0;
    static constexpr int kMaxConcurrent = 4;
    void sendRequest(const QueuedReq& r);
};
