#include "WindowsOcrEngine.h"

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Globalization.h>

#include <QBuffer>

using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage::Streams;

WindowsOcrEngine::WindowsOcrEngine(QObject* parent)
    : IOCREngine(parent) {
    m_watcher = new QFutureWatcher<OCRResult>(this);
    connect(m_watcher, &QFutureWatcher<OCRResult>::finished, this, [this]() {
        OCRResult result = m_watcher->result();
        emit recognitionComplete(result);
    });
}

bool WindowsOcrEngine::initialize(const QString& languageTag) {
    m_languageTag = languageTag;
    try {
        auto langs = OcrEngine::AvailableRecognizerLanguages();
        if (langs.Size() == 0) {
            emit recognitionError("No OCR languages available");
            return false;
        }
        return true;
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
        // Encode QImage as PNG into an in-memory buffer, then decode it
        // via BitmapDecoder to produce a SoftwareBitmap. This avoids manual
        // IMemoryBufferByteAccess COM interop for copying raw pixels.
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

        // Resolve language tag and create OcrEngine.
        // Language has no default constructor, so both resolution and
        // engine creation are folded into one block.
        OcrEngine engine(nullptr);
        auto const availableLangs = OcrEngine::AvailableRecognizerLanguages();

        if (m_languageTag != "auto") {
            winrt::Windows::Globalization::Language lang(
                m_languageTag.toStdWString());
            engine = OcrEngine::TryCreateFromLanguage(lang);
        }

        if (!engine && availableLangs.Size() > 0) {
            engine = OcrEngine::TryCreateFromLanguage(
                availableLangs.First().Current());
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
                    static_cast<int>(rect.Height)
                );
                result.boxes.append(box);
            }
        }
    } catch (winrt::hresult_error const& e) {
        emit recognitionError(QString::fromWCharArray(e.message().c_str()));
    }

    return result;
}
