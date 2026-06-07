#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QString>
#include "common/Types.h"

class TranslationOverlay : public QWidget {
    Q_OBJECT

public:
    explicit TranslationOverlay(QWidget* parent = nullptr);
    ~TranslationOverlay() override;

    void showTranslation(const LayoutResult& layout, const QString& text,
                         const StyleConfig& style);
    void updateStyle(const StyleConfig& style);
    int  overlayId() const { return m_id; }
    void setOverlayId(int id) { m_id = id; }
    void setInteractive(bool on);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    int  m_id = -1;
    QString m_text;
    StyleConfig m_style;
    bool m_interactive = false;
    bool m_flashGreen   = false;
    bool m_wordWrap     = false;  // paragraph mode: full-width, auto-wrap
    bool m_dragging     = false;
    bool m_dragMoved    = false;
    QPoint m_dragOffset;          // cursor offset from window top-left at press
    QPoint m_dragStartPos;        // global pos at press (for move threshold)
};
