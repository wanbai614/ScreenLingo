#pragma once

#include <QtCore/QRect>
#include <QtCore/QPoint>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QFont>

// --- Work Modes ---
enum class Mode {
    RealTime,
    Snapshot,
    Pause
};

// --- OCR Result ---
struct TextBox {
    QString text;
    QRect   boundingRect;  // relative to captured region
};

struct OCRResult {
    QString         fullText;
    QVector<TextBox> boxes;
};

// --- Translation ---
struct TranslateRequest {
    QString text;
    QString sourceLang;  // "auto" for auto-detect
    QString targetLang;
};

// --- Layout ---
struct LayoutRequest {
    QRect   sourceRect;        // original text screen position
    QString translatedText;
    int     preferredFontSize;
};

struct LayoutResult {
    QPoint  position;
    int     fontSize;
    int     maxWidth;
    bool    isTruncated;
};

// --- Style ---
struct StyleConfig {
    QColor textColor       = Qt::white;
    QColor backgroundColor = QColor(51, 51, 51, 179);  // #333 with ~70% alpha
    int    backgroundAlpha = 70;   // 0-100
    int    borderRadius    = 6;
    QColor borderColor     = QColor(102, 102, 102);
    int    borderWidth     = 1;
    QFont  font            = QFont("Microsoft YaHei", 14);
};

// --- Selection Area ---
struct SelectionArea {
    int   id;
    int   screenIndex;
    QRect geometry;
    bool  enabled = true;
};

// --- Hotkey ---
struct HotkeyBinding {
    QString id;
    QString label;
    QString defaultKeys;
    QString currentKeys;
};
