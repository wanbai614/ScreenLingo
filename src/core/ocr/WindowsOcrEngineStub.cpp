#include "WindowsOcrEngine.h"
#include <QtCore/QDebug>
#include <QtCore/QThread>
#include <QtCore/QTimer>

// Stub implementation for non-MSVC builds.
// The real WinRT-based implementation is in WindowsOcrEngine.cpp (MSVC only).

WindowsOcrEngine::WindowsOcrEngine(QObject* parent)
    : IOCREngine(parent) {}

bool WindowsOcrEngine::initialize(const QString& languageTag) {
    m_languageTag = languageTag;
    qWarning() << "WindowsOcrEngine: Stub mode — OCR not available. Build with MSVC for WinRT support.";
    return true;
}

void WindowsOcrEngine::recognize(const QImage& image) {
    Q_UNUSED(image);
    // Simulate async with empty result
    QTimer::singleShot(0, this, [this]() {
        emit recognitionComplete(OCRResult{});
    });
}
