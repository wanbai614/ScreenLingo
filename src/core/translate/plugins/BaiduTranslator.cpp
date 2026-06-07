#include "BaiduTranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrlQuery>
#include <QtCore/QCryptographicHash>
#include <QtCore/QRandomGenerator>

BaiduTranslator::BaiduTranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this)) {}

QVector<TranslatorConfigField> BaiduTranslator::configFields() const {
    return {
        {"appId",     "App ID",     "", false, true},
        {"secretKey", "Secret Key", "", true,  true},
    };
}

void BaiduTranslator::setConfig(const QString& key, const QString& value) {
    if (key == "appId")     m_appId     = value;
    if (key == "secretKey") m_secretKey = value;
}

QString BaiduTranslator::getConfig(const QString& key) const {
    if (key == "appId")     return m_appId;
    if (key == "secretKey") return m_secretKey;
    return {};
}

void BaiduTranslator::translate(const TranslateRequest& req) {
    if (m_appId.isEmpty() || m_secretKey.isEmpty()) {
        emit translationError(tr("Baidu App ID and Secret Key required"));
        return;
    }

    QString salt = QString::number(QRandomGenerator::global()->generate64());
    QString signStr = m_appId + req.text + salt + m_secretKey;
    QString sign = QCryptographicHash::hash(
        signStr.toUtf8(), QCryptographicHash::Md5).toHex();

    QUrlQuery params;
    params.addQueryItem("q",    req.text);
    params.addQueryItem("from", req.sourceLang == "auto" ? "auto" : req.sourceLang);
    params.addQueryItem("to",   req.targetLang);
    params.addQueryItem("appid", m_appId);
    params.addQueryItem("salt",  salt);
    params.addQueryItem("sign",  sign);

    QNetworkRequest request{QUrl(m_endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    QNetworkReply* reply = m_nam->post(
        request, params.toString(QUrl::FullyEncoded).toUtf8());
    m_pendingRequests.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, original = req.text]() {
        m_pendingRequests.removeOne(reply);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit translationError(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray results = doc.object().value("trans_result").toArray();
        QStringList parts;
        for (const auto& r : results)
            parts.append(r.toObject().value("dst").toString());
        QString translated = parts.join('\n');

        if (translated.isEmpty()) {
            QString errMsg = doc.object().value("error_msg").toString();
            emit translationError(errMsg.isEmpty() ? "Baidu: empty result" : errMsg);
            return;
        }
        emit translationReady(original, translated);
    });
}

void BaiduTranslator::cancelAll() {
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}
