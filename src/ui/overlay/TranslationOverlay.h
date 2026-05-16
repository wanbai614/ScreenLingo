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

protected:
    void paintEvent(QPaintEvent*) override;

private:
    int  m_id = -1;
    QString m_text;
    StyleConfig m_style;
};
