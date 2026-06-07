#pragma once

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QHash>
#include <QtCore/QVector>
#include "core/translate/ITranslator.h"

class LocalDictTranslator : public ITranslator {
    Q_OBJECT

public:
    explicit LocalDictTranslator(QObject* parent = nullptr);

    QString name() const override { return "LocalDict"; }
    QString category() const override { return "offline"; }
    bool isAvailable() const override { return true; }

    void translate(const TranslateRequest& req) override;
    void cancelAll() override;

    QVector<TranslatorConfigField> configFields() const override;
    void setConfig(const QString& key, const QString& value) override;
    QString getConfig(const QString& key) const override;

public slots:
    void downloadDictionary();

private:
    void loadDictionary();
    void ensureDefaultDict();
    QString lookupBest(const QString& text) const;

    QNetworkAccessManager* m_nam;
    QString m_dictPath;
    QString m_downloadUrl;
    QHash<QString, QString> m_dict;         // lowercase key → translation
    QVector<QNetworkReply*> m_pendingRequests;
};
