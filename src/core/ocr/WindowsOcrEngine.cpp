#include "WindowsOcrEngine.h"

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Globalization.h>

#include <QtCore/QMutexLocker>
#include <QBuffer>

using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Globalization;

// --- File-scope cached engine (created once, reused across frames) ---
static QMutex            s_engineMutex;
static OcrEngine         s_cachedEngine{ nullptr };

static OcrEngine createCachedEngine(const QString& languageTag) {
    auto available = OcrEngine::AvailableRecognizerLanguages();
    if (available.Size() == 0) return nullptr;

    if (languageTag != QStringLiteral("auto")) {
        Language lang(languageTag.toStdWString());
        auto engine = OcrEngine::TryCreateFromLanguage(lang);
        if (engine) return engine;
    }
    return OcrEngine::TryCreateFromLanguage(available.First().Current());
}

// =============================================================

WindowsOcrEngine::WindowsOcrEngine(QObject* parent)
    : IOCREngine(parent) {
    m_watcher = new QFutureWatcher<OCRResult>(this);
    connect(m_watcher, &QFutureWatcher<OCRResult>::finished, this, [this]() {
        emit recognitionComplete(m_watcher->result());
    });
}

bool WindowsOcrEngine::initialize(const QString& languageTag) {
    m_languageTag = languageTag;
    try {
        if (OcrEngine::AvailableRecognizerLanguages().Size() == 0) {
            emit recognitionError("No OCR languages available");
            return false;
        }
        // Pre-warm the cache on main thread
        QMutexLocker lock(&s_engineMutex);
        s_cachedEngine = createCachedEngine(languageTag);
        return (s_cachedEngine != nullptr);
    } catch (winrt::hresult_error const& e) {
        emit recognitionError(QString::fromWCharArray(e.message().c_str()));
        return false;
    }
}

void WindowsOcrEngine::recognize(const QImage& image) {
    auto future = QtConcurrent::run([this, image]() {
        return recognizeSync(image);
    });
    m_watcher->setFuture(future);
}

OCRResult WindowsOcrEngine::recognizeSync(const QImage& image) {
    OCRResult result;
    try {
        // PNG encoding → in-memory stream (reliable, portable)
        QByteArray pngBytes;
        QBuffer pngBuffer(&pngBytes);
        pngBuffer.open(QIODevice::WriteOnly);
        image.save(&pngBuffer, "PNG");
        pngBuffer.close();

        InMemoryRandomAccessStream stream;
        DataWriter dataWriter(stream);
        dataWriter.WriteBytes(winrt::array_view<const uint8_t>(
            reinterpret_cast<const uint8_t*>(pngBytes.constData()),
            static_cast<uint32_t>(pngBytes.size())));
        dataWriter.StoreAsync().get();
        dataWriter.DetachStream();
        stream.Seek(0);

        BitmapDecoder decoder = BitmapDecoder::CreateAsync(
            BitmapDecoder::PngDecoderId(), stream).get();
        SoftwareBitmap bitmap = decoder.GetFrameAsync(0).get()
                                    .GetSoftwareBitmapAsync().get();

        // Fast path: use cached engine. Slow path: create on demand.
        OcrEngine engine = s_cachedEngine;
        if (!engine) {
            QMutexLocker lock(&s_engineMutex);
            if (!s_cachedEngine)
                s_cachedEngine = createCachedEngine(m_languageTag);
            engine = s_cachedEngine;
        }

        if (!engine) {
            emit recognitionError("Failed to create OCR engine");
            return result;
        }

        OcrResult ocrResult = engine.RecognizeAsync(bitmap).get();
        result.fullText = QString::fromStdWString(ocrResult.Text().c_str());

        for (auto const& line : ocrResult.Lines()) {
            for (auto const& word : line.Words()) {
                TextBox box;
                box.text = QString::fromStdWString(word.Text().c_str());
                auto rect = word.BoundingRect();
                box.boundingRect = QRect(
                    static_cast<int>(rect.X),
                    static_cast<int>(rect.Y),
                    static_cast<int>(rect.Width),
                    static_cast<int>(rect.Height));
                result.boxes.append(box);
            }
        }
    } catch (winrt::hresult_error const& e) {
        emit recognitionError(QString::fromWCharArray(e.message().c_str()));
    }

    return result;
}
