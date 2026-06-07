#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtCore/QVector>
#include "common/Types.h"

/// Dialog for managing translator prompt presets (personas).
class PromptManagerDialog : public QDialog {
    Q_OBJECT

public:
    PromptManagerDialog(const QVector<PromptPreset>& presets,
                        const QString& activeId,
                        QWidget* parent = nullptr);

    QVector<PromptPreset> presets() const;
    QString               activeId() const;

private slots:
    void onSelectionChanged();
    void onAdd();
    void onDelete();
    void onDuplicate();

private:
    void refreshList();
    int  currentListRow() const;
    void saveCurrentEdits();

    QVector<PromptPreset> m_presets;
    QString               m_activeId;
    int                   m_currentEditRow = -1;  // row currently shown in editor

    QListWidget*    m_list        = nullptr;
    QLineEdit*      m_nameEdit    = nullptr;
    QPlainTextEdit* m_promptEdit  = nullptr;
    QPushButton*    m_addBtn      = nullptr;
    QPushButton*    m_deleteBtn   = nullptr;
    QPushButton*    m_dupBtn      = nullptr;
    QPushButton*    m_saveBtn     = nullptr;
    QPushButton*    m_okBtn       = nullptr;
    QPushButton*    m_cancelBtn   = nullptr;
};
