#pragma once

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QVector>
#include "core/translate/ITranslator.h"

class GoogleTranslator : public ITranslator {
    Q_OBJECT

public:
    explicit GoogleTranslator(QObject* parent = nullptr);

    QString name() const override { return "Google"; }
    QString category() const override { return "online"; }
    bool isAvailable() const override { return !m_apiKey.isEmpty(); }

    void translate(const TranslateRequest& req) override;
    void cancelAll() override;

    QVector<TranslatorConfigField> configFields() const override;
    void setConfig(const QString& key, const QString& value) override;
    QString getConfig(const QString& key) const override;

private:
    QNetworkAccessManager* m_nam;
    QString m_apiKey;
    QString m_endpoint = "https://translation.googleapis.com/language/translate/v2";
    QVector<QNetworkReply*> m_pendingRequests;
};
