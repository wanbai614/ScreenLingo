#pragma once

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QtCore/QMutex>
#include <QtCore/QDir>
#include <atomic>
#include "IOCREngine.h"

// ONNX Runtime types — opaque in header, full type in .cpp
namespace Ort { struct Env; struct Session; struct MemoryInfo; }

class PaddleOCREngine : public IOCREngine {
    Q_OBJECT

public:
    explicit PaddleOCREngine(QObject* parent = nullptr);
    ~PaddleOCREngine() override;

    bool initialize(const QString& languageTag = "auto") override;
    void recognize(const QImage& image) override;
    bool isModelReady() const;

private:
    OCRResult recognizeSync(const QImage& image);

    // ONNX Runtime handles (opaque pointers)
    void* m_env         = nullptr;
    void* m_detSession  = nullptr;
    void* m_recSession  = nullptr;
    void* m_memInfo     = nullptr;

    // Character set for CTC decode
    QStringList m_charset;

    bool loadModels(const QString& modelDir);
    void loadCharset(const QString& path);
    void embedDefaultCharset();

    // Preprocessing
    QImage preprocessDet(const QImage& img, int limitSide);
    QImage cropBox(const QImage& img, const QRect& box, int targetW, int targetH);

    // Postprocessing
    QVector<QRect> detPostprocess(const float* output, int oh, int ow,
                                   float scaleX, float scaleY,
                                   int origW, int origH);
    QString ctcDecode(const float* probs, int timeSteps, int numClasses);

    QString m_modelDir;
    QString m_languageTag;
    QFutureWatcher<OCRResult>* m_watcher = nullptr;
    QMutex m_mutex;
    std::atomic<bool> m_busy{false};
    bool m_ready = false;
    int  m_recModelClasses = 0;  // 97=EN model, 6625=ZH model

    // Detection model input size
    static constexpr int kDetLimitSide = 736;  // RapidOCR config: limit_side_len
    static constexpr int kRecImgWidth  = 640;   // wider → more chars per box (320→~25, 640→~50)
    static constexpr int kRecImgHeight = 48;
    static constexpr int kBlankId      = 0;
};
