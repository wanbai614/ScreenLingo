#pragma once

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include "IOCREngine.h"

class WindowsOcrEngine : public IOCREngine {
    Q_OBJECT

public:
    explicit WindowsOcrEngine(QObject* parent = nullptr);
    bool initialize(const QString& languageTag = "auto") override;
    void recognize(const QImage& image) override;

private:
    OCRResult recognizeSync(const QImage& image);
    QString m_languageTag;
    QFutureWatcher<OCRResult>* m_watcher = nullptr;
};
