#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>

class ICaptureEngine : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool initialize() = 0;
    virtual QImage captureRegion(const QRect& region, int screenIndex) = 0;
    virtual bool hasChanged(const QRect& region, int screenIndex) = 0;
    virtual void shutdown() = 0;

signals:
    void captureError(const QString& message);
};
