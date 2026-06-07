#pragma once

#include <QtCore/QRect>
#include <QtCore/QPoint>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtGui/QFontMetrics>
#include "common/Types.h"

class LayoutEngine {
public:
    LayoutResult compute(const LayoutRequest& request,
                         const QVector<QRect>& existingBubbles,
                         const QRect& screenBounds);

private:
    struct Candidate {
        QPoint position;
        int    priority;  // lower = better
    };

    QVector<Candidate> generateCandidates(const QRect& source, const QSize& bubbleSize,
                                           const QRect& screen);
    bool overlapsSource(const QRect& candidate, const QRect& source);
    bool overlapsExisting(const QRect& candidate, const QVector<QRect>& existing);
    QRect clampToScreen(const QRect& candidate, const QRect& screen);
    QSize estimateBubbleSize(const QString& text, const QFont& font, int maxWidth);

    static constexpr int kMinFontSize = 8;
    static constexpr int kGap         = 8;
};
