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
        int bubbleW    = std::min(textWidth + 24, screenBounds.width() - 10);
        int bubbleH    = textHeight + 16;
        QSize bubbleSize(bubbleW, bubbleH);

        // Candidates in priority order: right, right-above, right-below, below, left, above
        QPoint srcTL = request.sourceRect.topLeft();
        QPoint srcBR = request.sourceRect.bottomRight();
        QPoint srcCenter = request.sourceRect.center();

        struct { QPoint pos; int pri; } candidates[] = {
            // Right side (closest, aligned to source vertical center)
            {QPoint(srcBR.x() + kGap, srcCenter.y() - bubbleH / 2), 0},
            // Right-above (if right is occupied)
            {QPoint(srcBR.x() + kGap, srcTL.y() - bubbleH - kGap), 1},
            // Right-below
            {QPoint(srcBR.x() + kGap, srcBR.y() + kGap), 2},
            // Below (centered under source)
            {QPoint(srcTL.x(), srcBR.y() + kGap), 3},
            // Left side
            {QPoint(srcTL.x() - bubbleW - kGap, srcCenter.y() - bubbleH / 2), 4},
            // Above
            {QPoint(srcTL.x(), srcTL.y() - bubbleH - kGap), 5},
        };

        bool placed = false;
        for (const auto& cand : candidates) {
            QRect cr(cand.pos, bubbleSize);

            if (overlapsSource(cr, request.sourceRect)) continue;
            if (overlapsExisting(cr, existingBubbles)) continue;

            cr = clampToScreen(cr, screenBounds);

            if (overlapsSource(cr, request.sourceRect)) continue;
            if (overlapsExisting(cr, existingBubbles)) continue;

            result.position     = cr.topLeft();
            result.maxWidth     = cr.width();
            result.bubbleWidth  = cr.width();
            result.bubbleHeight = cr.height();
            result.fontSize     = fontSize;
            placed = true;
            break;
        }

        if (placed) return result;
        --fontSize;
    }

    // Last resort: truncate and force-place to the right
    result.fontSize    = kMinFontSize;
    result.isTruncated = true;

    QFont minFont("Microsoft YaHei", kMinFontSize);
    QFontMetrics fm(minFont);
    QString truncated = fm.elidedText(request.translatedText, Qt::ElideRight,
                                       screenBounds.width() - 20);
    int w = fm.horizontalAdvance(truncated) + 24;
    int h = fm.height() + 16;

    QPoint pos(request.sourceRect.right() + kGap, request.sourceRect.top());
    QRect cr(pos, QSize(w, h));
    cr = clampToScreen(cr, screenBounds);
    result.position     = cr.topLeft();
    result.maxWidth     = cr.width();
    result.bubbleWidth  = cr.width();
    result.bubbleHeight = cr.height();

    return result;
}

QSize LayoutEngine::estimateBubbleSize(const QString& text, const QFont& font, int maxWidth) {
    QFontMetrics fm(font);
    QRect rect = fm.boundingRect(QRect(0, 0, maxWidth, 10000),
                                  Qt::AlignLeft | Qt::TextWordWrap, text);
    return QSize(rect.width() + 20, rect.height() + 12);
}

bool LayoutEngine::overlapsSource(const QRect& candidate, const QRect& source) {
    return candidate.intersects(source);
}

bool LayoutEngine::overlapsExisting(const QRect& candidate,
                                     const QVector<QRect>& existing) {
    for (const auto& r : existing) {
        if (candidate.intersects(r.adjusted(-2, -2, 2, 2))) return true;
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
