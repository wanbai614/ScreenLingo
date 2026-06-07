#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QImage>
#include "common/Types.h"

struct TranslatorConfigField {
    QString key;
    QString label;
    QString defaultValue;
    bool    isSecret   = false;
    bool    isRequired = false;
};

class ITranslator : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    virtual QString name() const = 0;
    virtual QString category() const = 0;    // "online" | "llm"
    virtual bool isAvailable() const = 0;

    virtual void translate(const TranslateRequest& req) = 0;
    virtual void translateWithImage(const QImage& /*image*/,
                                     const QString& /*sourceLang*/,
                                     const QString& /*targetLang*/) {}
    virtual void cancelAll() = 0;

    virtual QVector<TranslatorConfigField> configFields() const = 0;
    virtual void setConfig(const QString& key, const QString& value) = 0;
    virtual QString getConfig(const QString& key) const = 0;

signals:
    void translationReady(const QString& original, const QString& translated);
    void translationError(const QString& error);
    // Vision-mode: single response with JSON containing all text+positions
    void visionResultReady(const QByteArray& json);
};
