#include "PaddleOCRSubprocessEngine.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QBuffer>
#include <QtCore/QTimer>
#include <QtCore/QDebug>

PaddleOCRSubprocessEngine::PaddleOCRSubprocessEngine(QObject* parent)
    : IOCREngine(parent) {}

PaddleOCRSubprocessEngine::~PaddleOCRSubprocessEngine() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool PaddleOCRSubprocessEngine::initialize(const QString& /*languageTag*/) {
    m_pythonExe = QCoreApplication::applicationDirPath() + "/../paddle_venv_ocr/Scripts/python.exe";
    if (!QFile::exists(m_pythonExe))
        m_pythonExe = "E:/XITONGHUANCUN/paddle_venv/Scripts/python.exe";
    if (!QFile::exists(m_pythonExe))
        m_pythonExe = "D:/Python/Python3.12/python.exe";

    m_serverScript = QCoreApplication::applicationDirPath() + "/paddleocr_server.py";
    if (!QFile::exists(m_serverScript)) {
        qWarning() << "PaddleOCR server script not found:" << m_serverScript;
        emit recognitionError("PaddleOCR server script not found");
        return false;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("FLAGS_use_mkldnn", "0");
    m_process->setProcessEnvironment(env);

    // Connect signals for async operation
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &PaddleOCRSubprocessEngine::onReadyRead);
    connect(m_process, &QProcess::errorOccurred,
            this, &PaddleOCRSubprocessEngine::onProcessError);
    connect(m_process, &QProcess::finished,
            this, &PaddleOCRSubprocessEngine::onProcessFinished);

    // Watchdog timer — 30s max for any async recognition
    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    connect(m_watchdog, &QTimer::timeout, this, [this]() {
        if (m_pending) {
            m_pending = false;
            emit recognitionError("PaddleOCR timeout");
        }
    });

    m_process->start(m_pythonExe, {m_serverScript});

    if (!m_process->waitForStarted(5000)) {
        qWarning() << "PaddleOCR process failed to start:" << m_process->errorString();
        emit recognitionError("PaddleOCR process failed to start");
        return false;
    }

    // Blocking wait for "ready" on stderr (only during init — brief)
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect(m_process, &QProcess::readyReadStandardError, &loop, [&]() {
        m_stderrBuf += QString::fromUtf8(m_process->readAllStandardError());
        if (m_stderrBuf.contains("ready")) loop.quit();
    });
    connect(m_process, &QProcess::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10000);
    loop.exec();

    if (!m_stderrBuf.contains("ready")) {
        qWarning() << "PaddleOCR server not ready after 10s";
        m_process->kill();
        m_process->waitForFinished(2000);
        emit recognitionError("PaddleOCR server not ready — check Python venv");
        return false;
    }

    m_ready = true;
    qInfo() << "PaddleOCR subprocess engine ready";
    return true;
}

void PaddleOCRSubprocessEngine::recognize(const QImage& image) {
    if (!m_ready || !m_process || m_process->state() != QProcess::Running) {
        emit recognitionError("PaddleOCR engine not ready");
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
    m_watchdog->start(30000);  // 30s async timeout
    m_process->write(reqLine);
    // Response will arrive in onReadyRead() — GUI thread stays responsive
}

void PaddleOCRSubprocessEngine::onReadyRead() {
    if (!m_pending) return;
    QByteArray respLine = m_process->readLine();
    if (respLine.isEmpty()) return;

    m_pending = false;
    m_watchdog->stop();

    QJsonDocument doc = QJsonDocument::fromJson(respLine);
    if (doc.isNull()) {
        emit recognitionError("PaddleOCR invalid response");
        return;
    }

    QJsonObject resp = doc.object();
    if (resp.contains("error") && !resp["error"].isNull()) {
        emit recognitionError(resp["error"].toString());
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

    QStringList all;
    for (const auto& b : result.boxes)
        all.append(b.text);
    result.fullText = all.join(' ');

    emit recognitionComplete(result);
}

void PaddleOCRSubprocessEngine::onProcessError(QProcess::ProcessError err) {
    Q_UNUSED(err);
    if (m_pending) {
        m_pending = false;
        m_watchdog->stop();
        emit recognitionError("PaddleOCR process error: " + m_process->errorString());
    }
}

void PaddleOCRSubprocessEngine::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode);
    if (m_pending) {
        m_pending = false;
        m_watchdog->stop();
        emit recognitionError(status == QProcess::CrashExit
            ? "PaddleOCR process crashed" : "PaddleOCR process exited unexpectedly");
    }
    m_ready = false;
}
