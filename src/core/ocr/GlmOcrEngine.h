#pragma once
#include "core/ocr/IOCREngine.h"
#include <QtCore/QProcess>
#include <QtCore/QTimer>

/// GLM-OCR via Python subprocess — cloud-based VLM document parsing.
/// Uses the GLM-OCR SDK (Zhipu API) with a local Python subprocess bridge.
class GlmOcrEngine : public IOCREngine {
    Q_OBJECT
public:
    explicit GlmOcrEngine(QObject* parent = nullptr);
    ~GlmOcrEngine() override;

    bool initialize(const QString& languageTag = "auto") override;
    void recognize(const QImage& image) override;

private slots:
    void onReadyRead();
    void onProcessError(QProcess::ProcessError err);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess* m_process = nullptr;
    QString   m_pythonExe;
    QString   m_serverScript;
    QString   m_stderrBuf;
    bool      m_ready = false;
    bool      m_pending = false;
    QTimer*   m_watchdog = nullptr;
};
