#include "OverlayManager.h"

OverlayManager::OverlayManager(QObject* parent) : QObject(parent) {}

OverlayManager::~OverlayManager() {
    qDeleteAll(m_overlays);
}

int OverlayManager::showTranslation(const LayoutResult& layout, const QString& text) {
    auto* overlay = new TranslationOverlay();
    overlay->setOverlayId(m_nextId);
    m_overlays[m_nextId] = overlay;

    overlay->showTranslation(layout, text, m_currentStyle);
    if (!m_globalVisible) overlay->hide();

    return m_nextId++;
}

void OverlayManager::updateTranslation(int id, const QString& newText,
                                        const LayoutResult& layout) {
    auto it = m_overlays.find(id);
    if (it == m_overlays.end()) return;
    it.value()->showTranslation(layout, newText, m_currentStyle);
}

void OverlayManager::removeTranslation(int id) {
    auto it = m_overlays.find(id);
    if (it == m_overlays.end()) return;
    delete it.value();
    m_overlays.remove(id);
}

void OverlayManager::removeAll() {
    qDeleteAll(m_overlays);
    m_overlays.clear();
}

void OverlayManager::showAll() {
    m_globalVisible = true;
    for (auto* overlay : m_overlays)
        overlay->show();
}

void OverlayManager::updateAllStyles(const StyleConfig& style) {
    m_currentStyle = style;
    for (auto* overlay : m_overlays)
        overlay->updateStyle(style);
}

QVector<QRect> OverlayManager::existingBubbleRects() const {
    QVector<QRect> rects;
    for (auto* overlay : m_overlays) {
        if (overlay->isVisible())
            rects.append(overlay->geometry());
    }
    return rects;
}
