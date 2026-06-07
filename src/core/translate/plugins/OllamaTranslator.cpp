#include "OllamaTranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>
#include <QtCore/QBuffer>

OllamaTranslator::OllamaTranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this)) {}

QVector<TranslatorConfigField> OllamaTranslator::configFields() const {
    return {
        {"baseUrl",      "Base URL",      "http://localhost:11434/v1", false, false},
        {"model",        "Model",         "llama3",                    false, false},
        {"systemPrompt", "System Prompt", m_systemPrompt,              false, false},
    };
}

void OllamaTranslator::setConfig(const QString& key, const QString& value) {
    if (key == "baseUrl")      m_baseUrl      = value;
    if (key == "model")        m_model        = value;
    if (key == "systemPrompt") m_systemPrompt = value;
}

QString OllamaTranslator::getConfig(const QString& key) const {
    if (key == "baseUrl")      return m_baseUrl;
    if (key == "model")        return m_model;
    if (key == "systemPrompt") return m_systemPrompt;
    return {};
}

void OllamaTranslator::translate(const TranslateRequest& req) {
    m_queue.append({req.text, req.sourceLang, req.targetLang, QImage{}, false, req.batchMode});
    processQueue();
}

void OllamaTranslator::processQueue() {
    while (m_activeRequests < kMaxConcurrent && !m_queue.isEmpty()) {
        QueuedReq r = m_queue.takeFirst();
        sendRequest(r);
    }
}

void OllamaTranslator::sendRequest(const QueuedReq& r) {
    ++m_activeRequests;
    QJsonObject body;
    body["model"] = m_model;

    if (r.isVlm) {
        // Ollama native vision API: images[] at top level, not content blocks
        QByteArray jpg;
        QBuffer buf(&jpg);
        buf.open(QIODevice::WriteOnly);
        r.image.save(&buf, "JPEG", 60);
        buf.close();
        QString b64 = QString::fromUtf8(jpg.toBase64());

        QString prompt = QString(
            "Detect ALL visible text in this screenshot. "
            "Translate each text element from %1 to %2.\n"
            "Output ONLY lines in this format (no other text):\n"
            "original_text|||translated_text|||x,y,w,h")
            .arg(r.srcLang, r.tgtLang);

        body["messages"] = QJsonArray{
            QJsonObject{{"role", "user"}, {"content", prompt}}
        };
        QJsonArray images;
        images.append(b64);
        body["images"] = images;
    } else if (r.batchMode) {
        // Batch mode: minimal system prompt — avoid verbose persona presets
        // that add 2000+ chars of glossary and cause the model to over-think.
        int count = r.text.count('\n') + 1;
        QString prompt = QStringLiteral(
            "Translate these %1 items from %2 to %3. "
            "Output ONLY a JSON array with %1 strings. Nothing else.\n"
            "Format: [\"t1\",\"t2\",...,\"t%1\"]\n\n%4")
            .arg(count).arg(r.srcLang, r.tgtLang).arg(r.text);
        body["messages"] = QJsonArray{
            QJsonObject{{"role", "user"}, {"content", prompt}}
        };
        body["format"] = QStringLiteral("json");
    } else {
        QString prompt = m_systemPrompt.arg(r.srcLang, r.tgtLang)
                         + QStringLiteral("\n\nOutput ONLY: {\"translation\":\"...\"}\n\nText: ") + r.text;
        body["messages"] = QJsonArray{
            QJsonObject{{"role", "user"}, {"content", prompt}}
        };
        body["format"] = QStringLiteral("json");  // force valid JSON output
    }
    body["stream"] = false;

    QUrl url(m_baseUrl + "/api/chat");
    qWarning() << "Ollama POST" << url.toString() << "model:" << m_model
               << (r.isVlm ? "VLM" : "text") << "len:" << (r.isVlm ? 0 : r.text.size());
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(r.batchMode ? 180000 : (r.isVlm ? 60000 : 15000));  // batch 180s, VLM 60s, text 15s

    QNetworkReply* reply = m_nam->post(request,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_pendingRequests.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, r]() {
        m_pendingRequests.removeOne(reply);
        --m_activeRequests;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Ollama ERROR:" << reply->errorString();
            emit translationError(reply->errorString());
            processQueue();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();
        QString content = obj.value("message").toObject()
            .value("content").toString().trimmed();

        if (r.isVlm) {
            // Try JSON array first
            auto tryJson = [&](const QByteArray& candidate) -> bool {
                QJsonDocument doc = QJsonDocument::fromJson(candidate);
                if (doc.isArray() && !doc.array().isEmpty()) {
                    emit visionResultReady(candidate);
                    return true;
                }
                return false;
            };
            if (tryJson(content.trimmed().toUtf8())) return;
            int s = content.indexOf('['), e = content.lastIndexOf(']');
            if (s >= 0 && e > s && tryJson(content.mid(s, e - s + 1).toUtf8())) return;

            // Try pipe format: original|||translated|||x,y,w,h
            QJsonArray arr;
            QStringList lines = content.split('\n');
            for (const QString& line : lines) {
                QStringList parts = line.split("|||");
                if (parts.size() < 2) continue;
                QString text = parts[0].trimmed();
                QString trans = parts[1].trimmed();
                // Skip prompt-echo lines
                if (text == trans || text == "original_text" || trans == "translated_text") continue;
                if (trans.isEmpty()) continue;
                QJsonArray bbox{0, 0, 100, 20};
                if (parts.size() >= 3) {
                    QStringList coords = parts[2].split(',');
                    if (coords.size() >= 4)
                        bbox = {coords[0].trimmed().toInt(), coords[1].trimmed().toInt(),
                                coords[2].trimmed().toInt(), coords[3].trimmed().toInt()};
                }
                arr.append(QJsonObject{
                    {"text", text}, {"bbox", bbox}, {"translation", trans}});
            }
            if (!arr.isEmpty()) {
                emit visionResultReady(QJsonDocument(arr).toJson(QJsonDocument::Compact));
                return;
            }

            // Fallback: entire response as one translation
            arr.append(QJsonObject{
                {"text", ""}, {"bbox", QJsonArray{0, 0, 100, 20}},
                {"translation", content.trimmed()}});
            emit visionResultReady(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        } else {
            qWarning() << "Ollama response OK, len:" << content.size();
            static const QRegularExpression thinkRx(
                QStringLiteral(R"(<\s*think\s*>[\s\S]*?<\s*/\s*think\s*>)"),
                QRegularExpression::CaseInsensitiveOption);
            content = content.replace(thinkRx, QString()).trimmed();
            emit translationReady(r.text, content);
        }
        processQueue();
    });
}

void OllamaTranslator::cancelAll() {
    m_queue.clear();
    m_activeRequests = 0;
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}

void OllamaTranslator::translateWithImage(const QImage& image,
                                            const QString& sourceLang,
                                            const QString& targetLang) {
    m_queue.append({QString(), sourceLang, targetLang, image, true, false});
    processQueue();
}
