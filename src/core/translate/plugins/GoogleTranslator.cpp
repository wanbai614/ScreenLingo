#include "GoogleTranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrlQuery>

GoogleTranslator::GoogleTranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this)) {}

QVector<TranslatorConfigField> GoogleTranslator::configFields() const {
    return {
        {"apiKey",  "API Key",  "", true,  true},
    };
}

void GoogleTranslator::setConfig(const QString& key, const QString& value) {
    if (key == "apiKey") m_apiKey = value;
}

QString GoogleTranslator::getConfig(const QString& key) const {
    if (key == "apiKey") return m_apiKey;
    return {};
}

void GoogleTranslator::translate(const TranslateRequest& req) {
    if (m_apiKey.isEmpty()) {
        emit translationError(tr("Google API key not configured"));
        return;
    }

    QJsonObject body;
    body["q"]    = req.text;
    body["source"] = req.sourceLang == "auto" ? QString() : req.sourceLang;
    body["target"] = req.targetLang;
    body["format"] = "text";

    QUrl url(m_endpoint);
    QUrlQuery query;
    query.addQueryItem("key", m_apiKey);
    url.setQuery(query);

    QNetworkRequest request{url};
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
        QJsonObject data = doc.object().value("data").toObject();
        QJsonArray translations = data.value("translations").toArray();
        if (translations.isEmpty()) {
            emit translationError("Google: empty translation result");
            return;
        }
        QString translated = translations[0].toObject()
            .value("translatedText").toString();
        emit translationReady(original, translated);
    });
}

void GoogleTranslator::cancelAll() {
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}
