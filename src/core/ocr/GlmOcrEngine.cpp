#include "GlmOcrEngine.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QBuffer>
#include <QtCore/QTimer>
#include <QtCore/QDebug>

GlmOcrEngine::GlmOcrEngine(QObject* parent)
    : IOCREngine(parent) {}

GlmOcrEngine::~GlmOcrEngine() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool GlmOcrEngine::initialize(const QString& /*languageTag*/) {
    m_pythonExe = QCoreApplication::applicationDirPath() + "/../paddle_venv_ocr/Scripts/python.exe";
    if (!QFile::exists(m_pythonExe))
        m_pythonExe = "E:/XITONGHUANCUN/paddle_venv/Scripts/python.exe";
    if (!QFile::exists(m_pythonExe))
        m_pythonExe = "D:/Python/Python3.12/python.exe";

    m_serverScript = QCoreApplication::applicationDirPath() + "/glmocr_server.py";
    if (!QFile::exists(m_serverScript)) {
        qWarning() << "GLM-OCR server script not found:" << m_serverScript;
        emit recognitionError("GLM-OCR server script not found");
        return false;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    m_process->setProcessEnvironment(env);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &GlmOcrEngine::onReadyRead);
    connect(m_process, &QProcess::errorOccurred,
            this, &GlmOcrEngine::onProcessError);
    connect(m_process, &QProcess::finished,
            this, &GlmOcrEngine::onProcessFinished);

    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    connect(m_watchdog, &QTimer::timeout, this, [this]() {
        if (m_pending) {
            m_pending = false;
            emit recognitionError("GLM-OCR timeout (60s)");
        }
    });

    m_process->start(m_pythonExe, {m_serverScript});

    if (!m_process->waitForStarted(5000)) {
        qWarning() << "GLM-OCR process failed to start:" << m_process->errorString();
        emit recognitionError("GLM-OCR process failed to start");
        return false;
    }

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
        qWarning() << "GLM-OCR server not ready after 15s";
        m_process->kill();
        m_process->waitForFinished(2000);
        emit recognitionError("GLM-OCR server not ready — check API key in config.yaml");
        return false;
    }

    m_ready = true;
    qInfo() << "GLM-OCR engine ready";
    return true;
}

void GlmOcrEngine::recognize(const QImage& image) {
    if (!m_ready || !m_process || m_process->state() != QProcess::Running) {
        emit recognitionError("GLM-OCR engine not ready");
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
    m_watchdog->start(60000);  // 60s — cloud API can be slow
    m_process->write(reqLine);
}

void GlmOcrEngine::onReadyRead() {
    if (!m_pending) return;
    QByteArray respLine = m_process->readLine();
    if (respLine.isEmpty()) return;

    m_pending = false;
    m_watchdog->stop();

    QJsonDocument doc = QJsonDocument::fromJson(respLine);
    if (doc.isNull()) {
        emit recognitionError("GLM-OCR invalid response");
        return;
    }

    QJsonObject resp = doc.object();
    if (resp.contains("error") && !resp["error"].isNull() && resp["error"].toString().size() > 0) {
        emit recognitionError("GLM-OCR: " + resp["error"].toString());
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
        // Build from boxes if fullText is empty
        QStringList all;
        for (const auto& b : result.boxes)
            all.append(b.text);
        result.fullText = all.join(' ');
    }

    emit recognitionComplete(result);
}

void GlmOcrEngine::onProcessError(QProcess::ProcessError err) {
    Q_UNUSED(err);
    if (m_pending) {
        m_pending = false;
        m_watchdog->stop();
        emit recognitionError("GLM-OCR process error: " + m_process->errorString());
    }
}

void GlmOcrEngine::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode);
    if (m_pending) {
        m_pending = false;
        m_watchdog->stop();
        emit recognitionError(status == QProcess::CrashExit
            ? "GLM-OCR process crashed" : "GLM-OCR process exited unexpectedly");
    }
    m_ready = false;
}
