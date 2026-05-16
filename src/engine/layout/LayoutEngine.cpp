#include "LayoutEngine.h"
#include <QtGui/QFontMetrics>

LayoutResult LayoutEngine::compute(const LayoutRequest& request,
                                    const QVector<QRect>& existingBubbles,
                                    const QRect& screenBounds) {
    LayoutResult result;
    result.isTruncated = false;

    int fontSize = request.preferredFontSize;
    QFont font("Microsoft YaHei", fontSize);

    while (fontSize >= kMinFontSize) {
        font.setPointSize(fontSize);
        QFontMetrics fm(font);
        int textWidth  = fm.horizontalAdvance(request.translatedText);
        int textHeight = fm.height();
        int bubbleW    = std::min(textWidth + 24, screenBounds.width() - 20);
        int bubbleH    = textHeight + 16;
        QSize bubbleSize(bubbleW, bubbleH);

        auto candidates = generateCandidates(request.sourceRect, bubbleSize, screenBounds);

        bool placed = false;
        for (const auto& cand : candidates) {
            QRect candidateRect(cand.position, bubbleSize);

            if (overlapsSource(candidateRect, request.sourceRect)) continue;
            if (overlapsExisting(candidateRect, existingBubbles)) continue;

            candidateRect = clampToScreen(candidateRect, screenBounds);
            result.position = candidateRect.topLeft();
            result.maxWidth = candidateRect.width();
            result.fontSize  = fontSize;
            placed = true;
            break;
        }

        if (placed) return result;
        --fontSize;
    }

    // Last resort: truncate and force place
    result.fontSize    = kMinFontSize;
    result.isTruncated = true;

    QFont minFont("Microsoft YaHei", kMinFontSize);
    QFontMetrics fm(minFont);
    QString truncated = fm.elidedText(request.translatedText, Qt::ElideRight,
                                       screenBounds.width() - 20);
    int w = fm.horizontalAdvance(truncated) + 24;
    int h = fm.height() + 16;

    QPoint pos(request.sourceRect.right() + kGap, request.sourceRect.top());
    QRect candidateRect(pos, QSize(w, h));
    candidateRect = clampToScreen(candidateRect, screenBounds);
    result.position = candidateRect.topLeft();
    result.maxWidth = candidateRect.width();

    return result;
}

QVector<LayoutEngine::Candidate> LayoutEngine::generateCandidates(
    const QRect& source, const QSize& bubbleSize, const QRect& screen) {

    QVector<Candidate> candidates;
    candidates.append({{source.right() + kGap, source.top()}, 0});  // right
    candidates.append({{source.left(), source.bottom() + kGap}, 1}); // below
    candidates.append({{source.left() - bubbleSize.width() - kGap, source.top()}, 2}); // left
    candidates.append({{source.left(), source.top() - bubbleSize.height() - kGap}, 3}); // above
    return candidates;
}

QSize LayoutEngine::estimateBubbleSize(const QString& text, const QFont& font, int maxWidth) {
    QFontMetrics fm(font);
    QRect rect = fm.boundingRect(QRect(0, 0, maxWidth, 10000),
                                  Qt::AlignLeft | Qt::TextWordWrap, text);
    return QSize(rect.width() + 24, rect.height() + 16);
}

bool LayoutEngine::overlapsSource(const QRect& candidate, const QRect& source) {
    return candidate.intersects(source);
}

bool LayoutEngine::overlapsExisting(const QRect& candidate,
                                     const QVector<QRect>& existing) {
    for (const auto& r : existing) {
        if (candidate.intersects(r)) return true;
    }
    return false;
}

QRect LayoutEngine::clampToScreen(const QRect& candidate, const QRect& screen) {
    QRect clamped = candidate;
    if (clamped.right() > screen.right())
        clamped.moveRight(screen.right());
    if (clamped.bottom() > screen.bottom())
        clamped.moveBottom(screen.bottom());
    if (clamped.left() < screen.left())
        clamped.moveLeft(screen.left());
    if (clamped.top() < screen.top())
        clamped.moveTop(screen.top());
    return clamped;
}
