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
    // Python executable in venv
    QString venvDir = QCoreApplication::applicationDirPath() + "/../paddle_venv_ocr";
    // Try the global venv location
    m_pythonExe = "E:/XITONGHUANCUN/paddle_venv/Scripts/python.exe";
    if (!QFile::exists(m_pythonExe)) {
        m_pythonExe = "D:/Python/Python3.12/python.exe";
    }

    m_serverScript = QCoreApplication::applicationDirPath() + "/paddleocr_server.py";
    if (!QFile::exists(m_serverScript)) {
        qWarning() << "PaddleOCR server script not found:" << m_serverScript;
        emit recognitionError("PaddleOCR server script not found");
        return false;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("FLAGS_use_mkldnn", "0");  // disable oneDNN to avoid PaddlePaddle 3.3.1 bug
    m_process->setProcessEnvironment(env);
    m_process->start(m_pythonExe, {m_serverScript});

    if (!m_process->waitForStarted(10000)) {
        qWarning() << "PaddleOCR process failed to start:" << m_process->errorString();
        emit recognitionError("PaddleOCR process failed to start");
        return false;
    }

    // Wait for "ready" signal on stderr
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    QString stderrBuf;
    connect(m_process, &QProcess::readyReadStandardError, &loop, [&]() {
        stderrBuf += QString::fromUtf8(m_process->readAllStandardError());
        if (stderrBuf.contains("ready")) {
            loop.quit();
        }
    });
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(120000); // 2 min timeout for first model download
    loop.exec();

    if (!stderrBuf.contains("ready")) {
        qWarning() << "PaddleOCR server not ready after timeout";
        emit recognitionError("PaddleOCR server timeout");
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

    // Send and read response
    m_process->write(reqLine);

    if (!m_process->waitForReadyRead(30000)) {
        emit recognitionError("PaddleOCR timeout");
        return;
    }

    QByteArray respLine = m_process->readLine();
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
        int x = bo["x"].toInt();
        int y = bo["y"].toInt();
        int w = bo["w"].toInt();
        int h = bo["h"].toInt();
        tb.boundingRect = QRect(x, y, w, h);
        result.boxes.append(tb);
    }

    // Build fullText
    QStringList all;
    for (const auto& b : result.boxes)
        all.append(b.text);
    result.fullText = all.join(' ');

    emit recognitionComplete(result);
}
