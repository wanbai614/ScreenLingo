#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <memory>
#include <vector>
#include "ITranslator.h"
#include "common/Types.h"

class TranslatorManager : public QObject {
    Q_OBJECT

public:
    explicit TranslatorManager(QObject* parent = nullptr);

    void registerPlugin(std::unique_ptr<ITranslator> plugin);
    ITranslator* active() const;
    QVector<ITranslator*> plugins() const;
    ITranslator* findByName(const QString& name) const;

    void setActive(const QString& name);
    void translate(const QString& text, const QString& sourceLang,
                   const QString& targetLang, bool batchMode = false);
    void translateWithImage(const QImage& image, const QString& sourceLang,
                            const QString& targetLang);
    void cancelAll();

signals:
    void translationReady(const QString& original, const QString& translated);
    void translationError(const QString& error);
    void visionResultReady(const QByteArray& json);
    void activeChanged(const QString& name);

private:
    void onPluginTranslationReady(const QString& original, const QString& translated);
    void onPluginTranslationError(const QString& error);

    std::vector<std::unique_ptr<ITranslator>> m_plugins;
    ITranslator* m_active = nullptr;
};
