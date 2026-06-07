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
    // Python executable — try venv first, then system Python
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
    m_process->start(m_pythonExe, {m_serverScript});

    if (!m_process->waitForStarted(5000)) {
        qWarning() << "PaddleOCR process failed to start:" << m_process->errorString();
        emit recognitionError("PaddleOCR process failed to start");
        return false;
    }

    // Async wait for "ready" — non-blocking on main thread (max 10s)
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect(m_process, &QProcess::readyReadStandardError, &loop, [&]() {
        m_stderrBuf += QString::fromUtf8(m_process->readAllStandardError());
        if (m_stderrBuf.contains("ready"))
            loop.quit();
    });
    // Also quit if process dies unexpectedly
    connect(m_process, &QProcess::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10000); // 10s — Python should start quickly if venv is set up
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

    // Encode image as PNG base64
    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    QJsonObject req;
    req["image"] = QString::fromLatin1(pngData.toBase64());
    QByteArray reqLine = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";

    m_process->write(reqLine);

    // Non-blocking wait with 15s timeout (was 30s)
    if (!m_process->waitForReadyRead(15000)) {
        emit recognitionError("PaddleOCR timeout");
        return;
    }

    QByteArray respLine = m_process->readLine();
    if (respLine.isEmpty()) {
        emit recognitionError("PaddleOCR empty response");
        return;
    }

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
