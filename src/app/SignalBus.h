#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>
#include "common/Types.h"

class SignalBus : public QObject {
    Q_OBJECT

public:
    explicit SignalBus(QObject* parent = nullptr) : QObject(parent) {}

signals:
    // Capture → OCR
    void frameReady(const QImage& frame, const QRect& region);

    // OCR → Translate
    void ocrCompleted(const OCRResult& result);

    // Translate → Layout → Overlay
    void translationReady(const QString& original, const QString& translated,
                          const QRect& sourceRect);

    // Mode changes
    void modeChanged(Mode newMode);

    // Style changes (Settings → all Overlays)
    void styleChanged(const StyleConfig& style);

    // Global visibility
    void globalVisibilityChanged(bool visible);

    // Translation errors
    void translationError(const QString& text, const QString& error);

    // Area selection completed
    void areaConfirmed(const SelectionArea& area);

    // Snapshot trigger
    void snapshotRequested();
};
