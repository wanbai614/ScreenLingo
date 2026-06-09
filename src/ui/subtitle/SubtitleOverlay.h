#pragma once

#include <QtWidgets/QWidget>
#include "common/Types.h"

/// Video-subtitle-style overlay bar at the bottom of the screen.
/// Semi-transparent black background, white centered text.
/// Click-through — never intercepts mouse events.
class SubtitleOverlay : public QWidget {
    Q_OBJECT
public:
    explicit SubtitleOverlay(QWidget* parent = nullptr);

    /// Display translated text as a subtitle bar. Empty string hides it.
    void showText(const QString& text, const StyleConfig& style);
    void hide();
    void clear();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString     m_text;
    StyleConfig m_style;
    int         m_screenWidth  = 1920;
    int         m_screenHeight = 1080;
};
