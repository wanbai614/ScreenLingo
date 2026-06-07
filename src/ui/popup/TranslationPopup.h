#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QString>

/// Floating popup showing translation near the cursor.
/// Click the ✕ button to dismiss; does NOT auto-close.
class TranslationPopup : public QWidget {
    Q_OBJECT

public:
    explicit TranslationPopup(QWidget* parent = nullptr);
    ~TranslationPopup() override;

    void showTranslation(const QPoint& screenPos,
                         const QString& original,
                         const QString& translated);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    QRect closeBtnRect() const;

    QString m_original;
    QString m_translated;
    bool    m_hoverClose = false;
    bool    m_dragging   = false;
    QPoint  m_dragOffset;

    static constexpr int kCloseBtnSize = 20;
};
