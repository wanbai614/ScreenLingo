#include "OllamaOcrEngine.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QBuffer>
#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QProcessEnvironment>

OllamaOcrEngine::OllamaOcrEngine(const QString& baseUrl, const QString& model,
                                   QObject* parent)
    : IOCREngine(parent), m_baseUrl(baseUrl), m_model(model) {}

OllamaOcrEngine::~OllamaOcrEngine() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool OllamaOcrEngine::initialize(const QString& /*languageTag*/) {
    m_pythonExe = QCoreApplication::applicationDirPath() + "/../paddle_venv_ocr/Scripts/python.exe";
    if (!QFile::exists(m_pythonExe))
        m_pythonExe = "E:/XITONGHUANCUN/paddle_venv/Scripts/python.exe";
    if (!QFile::exists(m_pythonExe))
        m_pythonExe = "D:/Python/Python3.12/python.exe";
    if (!QFile::exists(m_pythonExe))
        m_pythonExe = "python";

    m_serverScript = QCoreApplication::applicationDirPath() + "/paddleocrvl_server.py";
    if (!QFile::exists(m_serverScript)) {
        qWarning() << "PaddleOCR-VL server script not found:" << m_serverScript;
        return false;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("OLLAMA_BASE_URL", m_baseUrl);
    env.insert("PADDLEOCR_VL_MODEL", m_model);
    env.insert("http_proxy", "");
    env.insert("https_proxy", "");
    m_process->setProcessEnvironment(env);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &OllamaOcrEngine::onReadyRead);
    connect(m_process, &QProcess::errorOccurred,
            this, &OllamaOcrEngine::onProcessError);
    connect(m_process, &QProcess::finished,
            this, &OllamaOcrEngine::onProcessFinished);

    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    connect(m_watchdog, &QTimer::timeout, this, [this]() {
        if (m_pending) {
            m_pending = false;
            emit recognitionError("PaddleOCR-VL timeout (120s)");
        }
    });

    m_process->start(m_pythonExe, {m_serverScript});
    if (!m_process->waitForStarted(5000)) {
        qWarning() << "PaddleOCR-VL process failed to start:" << m_process->errorString();
        return false;
    }

    // Wait for "ready" on stderr
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect(m_process, &QProcess::readyReadStandardError, &loop, [&]() {
        m_stderrBuf += QString::fromUtf8(m_process->readAllStandardError());
        if (m_stderrBuf.contains("ready")) loop.quit();
    });
    connect(m_process, &QProcess::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    if (!m_stderrBuf.contains("ready")) {
        qWarning() << "PaddleOCR-VL server not ready after 15s:" << m_stderrBuf;
        m_process->kill();
        m_process->waitForFinished(2000);
        return false;
    }

    m_ready = true;
    qInfo() << "PaddleOCR-VL engine ready";
    return true;
}

void OllamaOcrEngine::recognize(const QImage& image) {
    if (!m_ready || !m_process || m_process->state() != QProcess::Running) {
        emit recognitionError("PaddleOCR-VL engine not ready");
        return;
    }

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    QJsonObject req;
    req["image"] = QString::fromLatin1(pngData.toBase64());
    QByteArray reqLine = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";

    m_pending = true;
    m_watchdog->start(120000);
    m_process->write(reqLine);
}

void OllamaOcrEngine::onReadyRead() {
    if (!m_pending) return;
    QByteArray respLine = m_process->readLine();
    if (respLine.isEmpty()) return;

    m_pending = false;
    m_watchdog->stop();

    QJsonDocument doc = QJsonDocument::fromJson(respLine);
    if (doc.isNull()) { emit recognitionError("PaddleOCR-VL invalid response"); return; }

    QJsonObject resp = doc.object();
    if (resp.contains("error") && !resp["error"].isNull() && !resp["error"].toString().isEmpty()) {
        emit recognitionError("PaddleOCR-VL: " + resp["error"].toString());
        return;
    }

    OCRResult result;
    QJsonArray boxes = resp["boxes"].toArray();
    for (const auto& b : boxes) {
        QJsonObject bo = b.toObject();
        TextBox tb;
        tb.text = bo["text"].toString();
        tb.boundingRect = QRect(bo["x"].toInt(), bo["y"].toInt(),
                                bo["w"].toInt(), bo["h"].toInt());
        result.boxes.append(tb);
    }
    result.fullText = resp["fullText"].toString();
    if (result.fullText.isEmpty()) {
        QStringList all;
        for (const auto& b : result.boxes) all.append(b.text);
        result.fullText = all.join(' ');
    }
    emit recognitionComplete(result);
}

void OllamaOcrEngine::onProcessError(QProcess::ProcessError err) {
    Q_UNUSED(err);
    if (m_pending) { m_pending = false; m_watchdog->stop(); }
}

void OllamaOcrEngine::onProcessFinished(int, QProcess::ExitStatus status) {
    if (m_pending) { m_pending = false; m_watchdog->stop(); }
    m_ready = false;
}
