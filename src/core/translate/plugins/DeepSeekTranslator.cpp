#include "DeepSeekTranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QRegularExpression>

DeepSeekTranslator::DeepSeekTranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this)) {}

QVector<TranslatorConfigField> DeepSeekTranslator::configFields() const {
    return {
        {"apiKey",       "API Key",       "",                          true,  true},
        {"baseUrl",      "Base URL",      "https://api.deepseek.com/v1", false, false},
        {"model",        "Model",         "deepseek-chat",             false, false},
        {"systemPrompt", "System Prompt", m_systemPrompt,              false, false},
    };
}

void DeepSeekTranslator::setConfig(const QString& key, const QString& value) {
    if (key == "apiKey")       m_apiKey       = value;
    if (key == "baseUrl")      m_baseUrl      = value;
    if (key == "model")        m_model        = value;
    if (key == "systemPrompt") m_systemPrompt = value;
}

QString DeepSeekTranslator::getConfig(const QString& key) const {
    if (key == "apiKey")       return m_apiKey;
    if (key == "baseUrl")      return m_baseUrl;
    if (key == "model")        return m_model;
    if (key == "systemPrompt") return m_systemPrompt;
    return {};
}

void DeepSeekTranslator::translate(const TranslateRequest& req) {
    if (m_apiKey.isEmpty()) {
        emit translationError(tr("DeepSeek API key not configured"));
        return;
    }

    QJsonObject body;
    body["model"] = m_model;

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", m_systemPrompt.arg(req.sourceLang, req.targetLang)}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", req.text}
    });
    body["messages"] = messages;
    body["temperature"] = 0.3;
    body["max_tokens"]  = 1024;
    body["stream"]      = false;

    QNetworkRequest request{QUrl(m_baseUrl + "/chat/completions")};
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
            emit translationError(tr("Empty response from DeepSeek"));
            return;
        }

        QString translated = choices[0].toObject()
            .value("message").toObject()
            .value("content").toString().trimmed();

        // Strip <think>...</think> blocks from reasoning models
        static const QRegularExpression thinkRx(
            QStringLiteral(R"(<\s*think\s*>[\s\S]*?<\s*/\s*think\s*>)"),
            QRegularExpression::CaseInsensitiveOption);
        translated = translated.replace(thinkRx, QString()).trimmed();

        emit translationReady(original, translated);
    });
}

void DeepSeekTranslator::cancelAll() {
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}
