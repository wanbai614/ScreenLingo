#pragma once

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QVector>
#include "core/translate/ITranslator.h"

class BaiduTranslator : public ITranslator {
    Q_OBJECT

public:
    explicit BaiduTranslator(QObject* parent = nullptr);

    QString name() const override { return "Baidu"; }
    QString category() const override { return "online"; }
    bool isAvailable() const override { return !m_appId.isEmpty() && !m_secretKey.isEmpty(); }

    void translate(const TranslateRequest& req) override;
    void cancelAll() override;

    QVector<TranslatorConfigField> configFields() const override;
    void setConfig(const QString& key, const QString& value) override;
    QString getConfig(const QString& key) const override;

private:
    QNetworkAccessManager* m_nam;
    QString m_appId;
    QString m_secretKey;
    QString m_endpoint = "https://fanyi-api.baidu.com/api/trans/vip/translate";
    QVector<QNetworkReply*> m_pendingRequests;
};
