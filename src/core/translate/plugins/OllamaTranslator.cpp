#include "OllamaTranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

OllamaTranslator::OllamaTranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this)) {}

QVector<TranslatorConfigField> OllamaTranslator::configFields() const {
    return {
        {"baseUrl", "Base URL", "http://localhost:11434/v1", false, false},
        {"model",   "Model",    "llama3",                    false, false},
    };
}

void OllamaTranslator::setConfig(const QString& key, const QString& value) {
    if (key == "baseUrl") m_baseUrl = value;
    if (key == "model")   m_model   = value;
}

QString OllamaTranslator::getConfig(const QString& key) const {
    if (key == "baseUrl") return m_baseUrl;
    if (key == "model")   return m_model;
    return {};
}

void OllamaTranslator::translate(const TranslateRequest& req) {
    QJsonObject body;
    body["model"] = m_model;

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", QString("Translate the following text from %1 to %2. "
                            "Output only the translation, nothing else.")
                        .arg(req.sourceLang, req.targetLang)}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", req.text}
    });
    body["messages"] = messages;
    body["temperature"] = 0.3;
    body["stream"]      = false;

    QNetworkRequest request{QUrl(m_baseUrl + "/chat/completions")};
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
            emit translationError(tr("Empty response from Ollama"));
            return;
        }

        QString translated = choices[0].toObject()
            .value("message").toObject()
            .value("content").toString().trimmed();
        emit translationReady(original, translated);
    });
}

void OllamaTranslator::cancelAll() {
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}
