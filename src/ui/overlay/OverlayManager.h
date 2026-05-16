#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QRect>
#include "TranslationOverlay.h"
#include "common/Types.h"

class OverlayManager : public QObject {
    Q_OBJECT

public:
    explicit OverlayManager(QObject* parent = nullptr);
    ~OverlayManager() override;

    int  showTranslation(const LayoutResult& layout, const QString& text);
    void updateTranslation(int id, const QString& newText, const LayoutResult& layout);
    void removeTranslation(int id);
    void removeAll();
    void showAll();
    void updateAllStyles(const StyleConfig& style);

    QVector<QRect> existingBubbleRects() const;

private:
    QHash<int, TranslationOverlay*> m_overlays;
    StyleConfig m_currentStyle;
    bool m_globalVisible = true;
    int  m_nextId = 1;
};
