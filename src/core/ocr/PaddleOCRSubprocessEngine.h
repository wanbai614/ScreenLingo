#pragma once
#include "core/ocr/IOCREngine.h"
#include <QtCore/QProcess>
#include <QtCore/QByteArray>

/// PaddleOCR via Python subprocess — guaranteed correct preprocessing.
class PaddleOCRSubprocessEngine : public IOCREngine {
    Q_OBJECT
public:
    explicit PaddleOCRSubprocessEngine(QObject* parent = nullptr);
    ~PaddleOCRSubprocessEngine() override;

    bool initialize(const QString& languageTag = "auto") override;
    void recognize(const QImage& image) override;

private:
    QProcess* m_process = nullptr;
    QString   m_pythonExe;
    QString   m_serverScript;
    QString   m_stderrBuf;       // accumulate stderr while waiting for "ready"
    bool      m_ready = false;
};
