#pragma once

#include "IOCREngine.h"
#include <QtCore/QProcess>
#include <QtCore/QTimer>

/// OCR engine backed by PaddleOCR-VL via Ollama (Python subprocess bridge).
/// Same architecture as GlmOcrEngine — QProcess + JSON-line stdin/stdout.
class OllamaOcrEngine : public IOCREngine {
    Q_OBJECT
public:
    explicit OllamaOcrEngine(const QString& baseUrl, const QString& model,
                              QObject* parent = nullptr);
    ~OllamaOcrEngine() override;

    bool initialize(const QString& languageTag = "auto") override;
    void recognize(const QImage& image) override;

private slots:
    void onReadyRead();
    void onProcessError(QProcess::ProcessError err);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess* m_process   = nullptr;
    QString   m_baseUrl;
    QString   m_model;
    QString   m_pythonExe;
    QString   m_serverScript;
    QString   m_stderrBuf;
    bool      m_ready      = false;
    bool      m_pending    = false;
    QTimer*   m_watchdog   = nullptr;
};
