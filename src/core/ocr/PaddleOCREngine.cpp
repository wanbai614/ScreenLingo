#include "PaddleOCREngine.h"

#include <onnxruntime_cxx_api.h>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QDateTime>
#include <QtCore/QTextStream>
#include <QtGui/QPainter>

// ===================================================================
//  PaddleOCR PP-OCRv4 character set — must match the model's dictionary
//  Downloaded from: https://paddleocr.bj.bcebos.com/ppocr_keys_v1.txt
//  Place in {models}/ppocr_keys_v1.txt alongside the ONNX model files.
// ===================================================================

// ===================================================================
//  PaddleOCREngine implementation
// ===================================================================

PaddleOCREngine::PaddleOCREngine(QObject* parent)
    : IOCREngine(parent) {
    m_watcher = new QFutureWatcher<OCRResult>(this);
    connect(m_watcher, &QFutureWatcher<OCRResult>::finished, this, [this]() {
        emit recognitionComplete(m_watcher->result());
    });
}

PaddleOCREngine::~PaddleOCREngine() {
    delete reinterpret_cast<Ort::Session*>(m_detSession);
    delete reinterpret_cast<Ort::Session*>(m_recSession);
    delete reinterpret_cast<Ort::MemoryInfo*>(m_memInfo);
    delete reinterpret_cast<Ort::Env*>(m_env);
}

bool PaddleOCREngine::initialize(const QString& languageTag) {
    m_languageTag = languageTag;

    // Model directory: {exe}/models or AppData
    m_modelDir = QCoreApplication::applicationDirPath() + "/models";
    QDir().mkpath(m_modelDir);

    // Load models FIRST — sets m_recModelClasses (EN=97, ZH=6625+)
    if (!loadModels(m_modelDir)) {
        emit recognitionError("Failed to load PaddleOCR models from " + m_modelDir);
        return false;
    }
    // THEN embed charset based on detected model type
    embedDefaultCharset();
    m_ready = true;
    return true;
}

bool PaddleOCREngine::isModelReady() const { return m_ready; }

void PaddleOCREngine::recognize(const QImage& image) {
    // Skip if already busy — avoid queuing stale frames in real-time mode
    if (m_busy) return;
    m_busy = true;
    auto future = QtConcurrent::run([this, image]() {
        OCRResult result = recognizeSync(image);
        m_busy = false;
        return result;
    });
    m_watcher->setFuture(future);
}

// --- Model loading ---
bool PaddleOCREngine::loadModels(const QString& modelDir) {
    try {
        auto* env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "ScreenLingoOCR");
        m_env = env;

        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(2);
        // Disable graph optimizations — RapidOCR ONNX models may break with ORT_ENABLE_ALL
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);

        // Try _infer suffix first (exported model), then _mobile (RapidOCR)
        QString detPath = modelDir + "/ch_PP-OCRv4_det_infer.onnx";
        if (!QFile::exists(detPath))
            detPath = modelDir + "/ch_PP-OCRv4_det_mobile.onnx";
        if (!QFile::exists(detPath)) return false;
        m_detSession = new Ort::Session(*env, detPath.toStdWString().c_str(), opts);

        QString recPath = modelDir + "/ch_PP-OCRv4_rec_infer.onnx";
        if (!QFile::exists(recPath))
            recPath = modelDir + "/ch_PP-OCRv4_rec_mobile.onnx";
        if (!QFile::exists(recPath)) return false;
        auto* recSess = new Ort::Session(*env, recPath.toStdWString().c_str(), opts);
        m_recSession = recSess;

        // Log model I/O for debugging
        {
            Ort::AllocatorWithDefaultOptions alloc;
            size_t nin = recSess->GetInputCount();
            size_t nout = recSess->GetOutputCount();
            qInfo() << "PaddleOCR rec model:" << nin << "inputs," << nout << "outputs";
            for (size_t i = 0; i < nin; ++i) {
                auto name = recSess->GetInputNameAllocated(i, alloc);
                auto info = recSess->GetInputTypeInfo(i);
                auto shape = info.GetTensorTypeAndShapeInfo().GetShape();
                QString s;
                for (auto d : shape) s += QString::number(d) + ",";
                qInfo() << "  input[" << i << "]:" << name.get() << "shape:" << s;
            }
            for (size_t i = 0; i < nout; ++i) {
                auto name = recSess->GetOutputNameAllocated(i, alloc);
                auto info = recSess->GetOutputTypeInfo(i);
                auto shape = info.GetTensorTypeAndShapeInfo().GetShape();
                QString s;
                for (auto d : shape) s += QString::number(d) + ",";
                qInfo() << "  output[" << i << "]:" << name.get() << "shape:" << s;
            }

            // Auto-detect EN vs ZH model from output shape
            if (nout > 0) {
                auto outTypeInfo = recSess->GetOutputTypeInfo(0);
                auto outTensorInfo = outTypeInfo.GetTensorTypeAndShapeInfo();
                auto outShape = outTensorInfo.GetShape();
                if (outShape.size() >= 3 && outShape[2] > 0 && outShape[2] <= 200) {
                    m_recModelClasses = outShape[2];
                    qInfo() << "PaddleOCR: EN recognition model ("
                            << m_recModelClasses << "classes)";
                }
            }
        }

        m_memInfo = new Ort::MemoryInfo(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));

        return true;
    } catch (const Ort::Exception& e) {
        qWarning() << "ONNX Runtime error:" << e.what();
        return false;
    }
}

// --- Character set ---
void PaddleOCREngine::embedDefaultCharset() {
    // EN model: PP-OCRv4_rec_en charset — MUST match training order
    // indices: 0=blank, 1-10=0-9, 11-36=A-Z, 37-62=a-z, 63-96=specials
    if (m_recModelClasses > 0 && m_recModelClasses <= 200) {
        qInfo() << "PaddleOCR: using embedded EN charset (97 classes)";
        m_charset = {
            "",   // 0 = CTC blank
            "0","1","2","3","4","5","6","7","8","9",               // 1-10
            "A","B","C","D","E","F","G","H","I","J","K","L","M",   // 11-23
            "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",   // 24-36
            "a","b","c","d","e","f","g","h","i","j","k","l","m",   // 37-49
            "n","o","p","q","r","s","t","u","v","w","x","y","z",   // 50-62
            "!","\"","#","$","%","&","'","(",")","*","+",",","-",".","/",  // 63-77
            ":",";","<","=",">","?","@",                            // 78-84
            "[","\\","]","^","_","`",                                // 85-90
            "{","|","}","~"," "                                       // 91-96
        };
        return;
    }

    // ZH model: load charset from file
    QString path = m_modelDir + "/ppocr_keys_v1.txt";
    if (QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_charset.append("");  // idx 0 = blank (CTC)
            while (!f.atEnd())
                m_charset.append(QString::fromUtf8(f.readLine()).trimmed());
            m_charset.append(" ");  // space at end (RapidOCR convention)
            qInfo() << "PaddleOCR: loaded" << m_charset.size() << "chars from" << path;
            return;
        }
    }
    // Fallback: EN charset
    qWarning() << "PaddleOCR: charset file not found, using embedded EN charset";
    m_charset = {"", " "};
    for (char c = '0'; c <= '9'; ++c) m_charset.append(QChar(c));
    for (char c = 'a'; c <= 'z'; ++c) m_charset.append(QChar(c));
    for (char c = 'A'; c <= 'Z'; ++c) m_charset.append(QChar(c));
}

// ===================================================================
//  Detection preprocessing
//  Resize: limit MIN side to kDetLimitSide, keep aspect ratio
// ===================================================================
QImage PaddleOCREngine::preprocessDet(const QImage& img, int limitSide) {
    int w = img.width(), h = img.height();
    float ratio = 1.0f;
    if (qMin(w, h) < limitSide) {
        ratio = (float)limitSide / qMin(w, h);
        w = qMax((int)(w * ratio), 1);
        h = qMax((int)(h * ratio), 1);
    }
    // Round to multiples of 32
    w = ((w + 31) / 32) * 32;
    h = ((h + 31) / 32) * 32;

    return img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
              .convertToFormat(QImage::Format_RGB32);
}

// ===================================================================
//  Detection postprocess: probability map → text boxes
//  Flood-fill from high-confidence seeds, then grow & merge.
// ===================================================================
QVector<QRect> PaddleOCREngine::detPostprocess(
    const float* output, int oh, int ow,
    float scaleX, float scaleY, int origW, int origH)
{
    // Phase 1: find seed pixels (very high confidence → definitely text)
    const float kSeedThresh  = 0.7f;
    const float kGrowThresh  = 0.3f;
    const int   kMinArea     = qMax(origW * origH / 2000, 40);

    int mapSize = oh * ow;
    QVector<uchar> visited(mapSize, 0);
    QVector<QRect> boxes;

    // 8-connected neighbor offsets
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int y = 0; y < oh; ++y) {
        for (int x = 0; x < ow; ++x) {
            int idx = y * ow + x;
            if (visited[idx] || output[idx] < kSeedThresh) continue;

            // Flood-fill from seed, collecting region bounds
            int l = x, r = x, t = y, b = y, area = 0;
            QVector<int> stack;
            stack.append(idx);
            visited[idx] = 1;

            while (!stack.isEmpty()) {
                int cur = stack.takeLast();
                int cx = cur % ow, cy = cur / ow;
                ++area;
                l = qMin(l, cx); r = qMax(r, cx);
                t = qMin(t, cy); b = qMax(b, cy);

                for (int n = 0; n < 8; ++n) {
                    int nx = cx + dx[n], ny = cy + dy[n];
                    if (nx < 0 || nx >= ow || ny < 0 || ny >= oh) continue;
                    int ni = ny * ow + nx;
                    if (!visited[ni] && output[ni] >= kGrowThresh) {
                        visited[ni] = 1;
                        stack.append(ni);
                    }
                }
            }

            if (area >= kMinArea) {
                // Unclip: expand box proportionally, then clip to bounds
                int cx = (l + r) / 2, cy = (t + b) / 2;
                int uw = (int)((r - l + 1) * 1.3f);
                int uh = (int)((b - t + 1) * 1.3f);
                int ux = qMax(0, cx - uw / 2);
                int uy = qMax(0, cy - uh / 2);
                uw = qMin(ow - ux, uw);
                uh = qMin(oh - uy, uh);

                // Scale to original coordinates
                int ox = (int)(ux * scaleX);
                int oy = (int)(uy * scaleY);
                int ow2 = (int)(uw * scaleX);
                int oh2 = (int)(uh * scaleY);
                ox = qMax(0, ox - 1);
                oy = qMax(0, oy - 1);
                ow2 = qMin(origW - ox, ow2 + 2);
                oh2 = qMin(origH - oy, oh2 + 2);
                boxes.append(QRect(ox, oy, ow2, oh2));
            }
        }
    }

    // Merge highly overlapping boxes
    QVector<QRect> merged;
    QVector<bool> used(boxes.size(), false);
    for (int i = 0; i < boxes.size(); ++i) {
        if (used[i]) continue;
        QRect cur = boxes[i];
        for (int j = i + 1; j < boxes.size(); ++j) {
            if (used[j]) continue;
            QRect inter = cur.intersected(boxes[j]);
            if (inter.width() * inter.height() > cur.width() * cur.height() / 3) {
                cur = cur.united(boxes[j]);
                used[j] = true;
            }
        }
        merged.append(cur);
    }
    return merged;
}

// ===================================================================
//  Recognition preprocessing: crop box, resize to fixed size
// ===================================================================
QImage PaddleOCREngine::cropBox(const QImage& img, const QRect& box,
                                  int targetW, int targetH) {
    QRect clamped = box.intersected(img.rect());
    if (clamped.isEmpty()) return QImage(targetW, targetH, QImage::Format_RGB32);

    QImage crop = img.copy(clamped).convertToFormat(QImage::Format_RGB32);
    int w = crop.width(), h = crop.height();

    // Match PaddleOCR resize_norm_img: fix height to targetH, compute proportional width
    float ratio = (float)w / h;
    int resizedW = (int)std::ceil(targetH * ratio);
    if (resizedW > targetW)
        resizedW = targetW;
    resizedW = qMax(resizedW, 1);

    // Resize to (resizedW, targetH) — height fixed, width proportional
    QImage scaled = crop.scaled(resizedW, targetH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Pad to target size with zeros (black → -1 after normalization)
    QImage result(targetW, targetH, QImage::Format_RGB32);
    result.fill(Qt::black);
    QPainter p(&result);
    p.drawImage(0, 0, scaled);  // left-align
    p.end();
    return result;
}

// ===================================================================
//  CTC greedy decoder
// ===================================================================
QString PaddleOCREngine::ctcDecode(const float* probs, int timeSteps, int numClasses) {
    // Use the actual model output class count (e.g., 97 for EN, 6625 for ZH),
    // NOT m_charset.size(), to avoid reading beyond valid probability data.
    int charsetSize = m_charset.size();
    QString result;
    int lastChar = kBlankId;
    for (int t = 0; t < timeSteps; ++t) {
        int bestClass = (int)(std::max_element(probs, probs + numClasses) - probs);
        probs += numClasses;
        if (bestClass != kBlankId && bestClass != lastChar && bestClass < charsetSize)
            result += m_charset[bestClass];
        lastChar = bestClass;
    }
    return result;
}

// ===================================================================
//  Main recognition pipeline
// ===================================================================
OCRResult PaddleOCREngine::recognizeSync(const QImage& image) {
    OCRResult result;
    if (!m_ready) return result;

    QMutexLocker lock(&m_mutex);
    auto& memInfo = *reinterpret_cast<Ort::MemoryInfo*>(m_memInfo);
    auto& detSession = *reinterpret_cast<Ort::Session*>(m_detSession);
    auto& recSession = *reinterpret_cast<Ort::Session*>(m_recSession);

    try {
        int imgW = image.width();
        int imgH = image.height();

        // ---- Step 1: Preprocess for detection ----
        QImage detImg = preprocessDet(image, kDetLimitSide);
        int detW = detImg.width();
        int detH = detImg.height();

        // PP-OCRv4 detection: BGR input → (x/255 - 0.5) / 0.5 → range [-1, 1]
        // Verified against official PaddleOCR resize_norm_img() source code
        std::vector<float> detInput(3 * detH * detW);
        for (int y = 0; y < detH; ++y) {
            const uchar* src = detImg.constScanLine(y);
            for (int x = 0; x < detW; ++x) {
                int idx = y * detW + x;
                int off = x * 4;
                detInput[0 * detH * detW + idx] = (src[off+0] / 255.0f - 0.5f) / 0.5f;  // B
                detInput[1 * detH * detW + idx] = (src[off+1] / 255.0f - 0.5f) / 0.5f;  // G
                detInput[2 * detH * detW + idx] = (src[off+2] / 255.0f - 0.5f) / 0.5f;  // R
            }
        }

        // ---- Step 2: Run detection model ----
        std::vector<int64_t> detShape{ 1, 3, detH, detW };
        Ort::Value detTensor = Ort::Value::CreateTensor<float>(
            memInfo, detInput.data(), detInput.size(), detShape.data(), detShape.size());

        Ort::AllocatorWithDefaultOptions alloc;
        auto detInName  = detSession.GetInputNameAllocated(0, alloc);
        auto detOutName = detSession.GetOutputNameAllocated(0, alloc);
        const char* detInNames[]  = { detInName.get() };
        const char* detOutNames[] = { detOutName.get() };
        auto detOutputs = detSession.Run(Ort::RunOptions{ nullptr },
            detInNames, &detTensor, 1, detOutNames, 1);

        float* detData = detOutputs[0].GetTensorMutableData<float>();
        auto detOutShape = detOutputs[0].GetTensorTypeAndShapeInfo().GetShape();
        int outCh = (int)detOutShape[1];
        int outH  = (int)detOutShape[2];
        int outW  = (int)detOutShape[3];
        qDebug() << "PaddleOCR det output: ch=" << outCh << "h=" << outH << "w=" << outW
                 << "input:" << detW << "x" << detH << "orig:" << imgW << "x" << imgH;

        float scaleX = (float)imgW / outW;
        float scaleY = (float)imgH / outH;

        QVector<QRect> detBoxes = detPostprocess(detData, outH, outW,
                                                   scaleX, scaleY, imgW, imgH);
        qDebug() << "PaddleOCR detected" << detBoxes.size() << "boxes";

        // ---- Step 3: Recognize each detected box ----
        if (detBoxes.isEmpty()) return result;

        auto& recMemInfo = *reinterpret_cast<Ort::MemoryInfo*>(m_memInfo);
        auto& recSession = *reinterpret_cast<Ort::Session*>(m_recSession);
        for (const auto& box : detBoxes) {
            QImage crop = cropBox(image, box, kRecImgWidth, kRecImgHeight);

            // PP-OCRv4 recognition: BGR → (x/255 - 0.5) / 0.5 → [-1, 1]
            // Verified against official PaddleOCR resize_norm_img() source code
            std::vector<float> recInput(3 * kRecImgHeight * kRecImgWidth);
            for (int y = 0; y < kRecImgHeight; ++y) {
                const uchar* src = crop.constScanLine(y);
                for (int x = 0; x < kRecImgWidth; ++x) {
                    int idx = y * kRecImgWidth + x;
                    int off = x * 4;
                    recInput[0 * kRecImgHeight * kRecImgWidth + idx] = (src[off+0] / 255.0f - 0.5f) / 0.5f;  // B
                    recInput[1 * kRecImgHeight * kRecImgWidth + idx] = (src[off+1] / 255.0f - 0.5f) / 0.5f;  // G
                    recInput[2 * kRecImgHeight * kRecImgWidth + idx] = (src[off+2] / 255.0f - 0.5f) / 0.5f;  // R
                }
            }

            std::vector<int64_t> recShape{ 1, 3, kRecImgHeight, kRecImgWidth };
            Ort::Value recTensor = Ort::Value::CreateTensor<float>(
                recMemInfo, recInput.data(), recInput.size(),
                recShape.data(), recShape.size());

            auto recInName  = recSession.GetInputNameAllocated(0, alloc);
            auto recOutName = recSession.GetOutputNameAllocated(0, alloc);
            const char* recInNames[]  = { recInName.get() };
            const char* recOutNames[] = { recOutName.get() };
            auto recOutputs = recSession.Run(Ort::RunOptions{ nullptr },
                recInNames, &recTensor, 1, recOutNames, 1);

            float* recData = recOutputs[0].GetTensorMutableData<float>();
            auto recShapeOut = recOutputs[0].GetTensorTypeAndShapeInfo().GetShape();
            int timeSteps = (int)recShapeOut[1];
            int modelClasses = (int)recShapeOut[2];

            // Diagnostic: dump raw output for first box to log file
            static int s_dumpCount = 0;
            if (s_dumpCount++ == 0) {
                QString dump;
                int nc = qMin(modelClasses, 100);
                for (int t = 0; t < qMin(5, timeSteps); ++t) {
                    float* tp = recData + t * modelClasses;
                    int top[3] = {};
                    float topV[3] = {-999, -999, -999};
                    for (int c = 0; c < nc; ++c) {
                        if (tp[c] > topV[0]) { topV[2]=topV[1]; top[2]=top[1]; topV[1]=topV[0]; top[1]=top[0]; topV[0]=tp[c]; top[0]=c; }
                        else if (tp[c] > topV[1]) { topV[2]=topV[1]; top[2]=top[1]; topV[1]=tp[c]; top[1]=c; }
                        else if (tp[c] > topV[2]) { topV[2]=tp[c]; top[2]=c; }
                    }
                    dump += QString("t%1[%2:%.4f %3:%.4f %4:%.4f] ")
                        .arg(t).arg(top[0]).arg(topV[0]).arg(top[1]).arg(topV[1]).arg(top[2]).arg(topV[2]);
                }
                // Write to a separate file (main log is locked by appLog)
                QString logPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                                  + "/paddleocr_raw.log";
                QFile lf(logPath);
                if (lf.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                    QTextStream ts(&lf);
                    ts << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << "  "
                       << "PADDLEOCR_RAW: ts=" << timeSteps << " nc=" << modelClasses
                       << " cs=" << m_charset.size() << " " << dump << "\n";
                }
            }

            QString text = ctcDecode(recData, timeSteps, modelClasses).trimmed();
            if (text.isEmpty()) continue;

            TextBox tb;
            tb.text = text;
            tb.boundingRect = box;
            result.boxes.append(tb);
        }

        // Build fullText
        QStringList all;
        for (const auto& b : result.boxes) all.append(b.text);
        result.fullText = all.join(' ');

    } catch (const Ort::Exception& e) {
        emit recognitionError(QString("ONNX error: %1").arg(e.what()));
    }

    return result;
}
