#include "OpenAITranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

OpenAITranslator::OpenAITranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this)) {}

QVector<TranslatorConfigField> OpenAITranslator::configFields() const {
    return {
        {"apiKey",       "API Key",       "",                       true,  true},
        {"baseUrl",      "Base URL",      "https://api.openai.com/v1", false, false},
        {"model",        "Model",         "gpt-4o",                 false, false},
        {"systemPrompt", "System Prompt", m_systemPrompt,            false, false},
    };
}

void OpenAITranslator::setConfig(const QString& key, const QString& value) {
    if (key == "apiKey")       m_apiKey       = value;
    if (key == "baseUrl")      m_baseUrl      = value;
    if (key == "model")        m_model        = value;
    if (key == "systemPrompt") m_systemPrompt = value;
}

QString OpenAITranslator::getConfig(const QString& key) const {
    if (key == "apiKey")       return m_apiKey;
    if (key == "baseUrl")      return m_baseUrl;
    if (key == "model")        return m_model;
    if (key == "systemPrompt") return m_systemPrompt;
    return {};
}

void OpenAITranslator::translate(const TranslateRequest& req) {
    if (m_apiKey.isEmpty()) {
        emit translationError(tr("OpenAI API key not configured"));
        return;
    }

    QJsonObject body;
    body["model"] = m_model;

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", m_systemPrompt}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", req.text}
    });
    body["messages"] = messages;
    body["temperature"] = 0.3;

    QNetworkRequest request(QUrl(m_baseUrl + "/chat/completions"));
    request.setRawHeader("Authorization",
                         QString("Bearer %1").arg(m_apiKey).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_nam->post(request,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_pendingRequests.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, original = req.text]() {
        m_pendingRequests.removeOne(reply);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit translationError(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray choices = doc.object().value("choices").toArray();
        if (choices.isEmpty()) {
            emit translationError(tr("Empty LLM response"));
            return;
        }

        QString translated = choices[0].toObject()
            .value("message").toObject()
            .value("content").toString().trimmed();
        emit translationReady(original, translated);
    });
}

void OpenAITranslator::cancelAll() {
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}
