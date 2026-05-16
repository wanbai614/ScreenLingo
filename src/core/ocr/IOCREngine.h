#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>
#include "common/Types.h"

class IOCREngine : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool initialize(const QString& languageTag = "auto") = 0;
    virtual void recognize(const QImage& image) = 0;

signals:
    void recognitionComplete(const OCRResult& result);
    void recognitionError(const QString& error);
};
