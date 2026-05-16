#include "DeepLTranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrlQuery>

DeepLTranslator::DeepLTranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this)) {}

QVector<TranslatorConfigField> DeepLTranslator::configFields() const {
    return {
        {"apiKey",  "API Key",   "",  true,  true},
        {"endpoint","Endpoint",  "https://api-free.deepl.com/v2/translate", false, false},
    };
}

void DeepLTranslator::setConfig(const QString& key, const QString& value) {
    if (key == "apiKey")   m_apiKey   = value;
    if (key == "endpoint") m_endpoint = value;
}

QString DeepLTranslator::getConfig(const QString& key) const {
    if (key == "apiKey")   return m_apiKey;
    if (key == "endpoint") return m_endpoint;
    return {};
}

void DeepLTranslator::translate(const TranslateRequest& req) {
    if (m_apiKey.isEmpty()) {
        emit translationError("DeepL API key not configured");
        return;
    }

    QUrlQuery postData;
    postData.addQueryItem("text", req.text);
    postData.addQueryItem("target_lang", req.targetLang.toUpper());
    if (req.sourceLang != "auto" && !req.sourceLang.isEmpty()) {
        postData.addQueryItem("source_lang", req.sourceLang.toUpper());
    }

    QNetworkRequest request{QUrl(m_endpoint)};
    request.setRawHeader("Authorization",
                         QString("DeepL-Auth-Key %1").arg(m_apiKey).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    QNetworkReply* reply = m_nam->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    m_pendingRequests.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, original = req.text]() {
        m_pendingRequests.removeOne(reply);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit translationError(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray translations = doc.object().value("translations").toArray();
        if (translations.isEmpty()) {
            emit translationError("Empty translation result");
            return;
        }

        QString translated = translations[0].toObject().value("text").toString();
        emit translationReady(original, translated);
    });
}

void DeepLTranslator::cancelAll() {
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}
