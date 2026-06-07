#pragma once
#include "core/ocr/IOCREngine.h"
#include <QtCore/QProcess>
#include <QtCore/QByteArray>
#include <QtCore/QTimer>

/// PaddleOCR via Python subprocess — guaranteed correct preprocessing.
/// Recognition is ASYNC (signal-driven) to avoid blocking the GUI thread.
class PaddleOCRSubprocessEngine : public IOCREngine {
    Q_OBJECT
public:
    explicit PaddleOCRSubprocessEngine(QObject* parent = nullptr);
    ~PaddleOCRSubprocessEngine() override;

    bool initialize(const QString& languageTag = "auto") override;
    void recognize(const QImage& image) override;

private slots:
    void onReadyRead();
    void onProcessError(QProcess::ProcessError err);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void restartProcess();

    QProcess* m_process = nullptr;
    QString   m_pythonExe;
    QString   m_serverScript;
    QString   m_stderrBuf;
    bool      m_ready = false;
    bool      m_pending = false;   // waiting for a response
    QTimer*   m_watchdog = nullptr; // timeout guard for async requests
};
