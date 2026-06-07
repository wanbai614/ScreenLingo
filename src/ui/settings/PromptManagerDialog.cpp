#include "PromptManagerDialog.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtCore/QUuid>

PromptManagerDialog::PromptManagerDialog(const QVector<PromptPreset>& presets,
                                           const QString& activeId,
                                           QWidget* parent)
    : QDialog(parent), m_presets(presets), m_activeId(activeId) {

    setWindowTitle(tr("Manage Translation Prompts"));
    setMinimumSize(780, 520);

    // --- Left: list + set-active button ---
    auto* leftPanel = new QVBoxLayout;
    leftPanel->setContentsMargins(0, 0, 0, 0);

    auto* listLabel = new QLabel(tr("Prompts:"));
    listLabel->setStyleSheet("font-weight: 600; color: #8b92a5; font-size: 11px; padding: 2px 4px;");
    leftPanel->addWidget(listLabel);

    m_list = new QListWidget;
    m_list->setMinimumWidth(190);
    leftPanel->addWidget(m_list, 1);

    // --- Right: editor ---
    auto* right = new QVBoxLayout;
    right->setContentsMargins(8, 0, 0, 0);

    auto* nameLabel = new QLabel(tr("Name:"));
    nameLabel->setStyleSheet("font-weight: 600; color: #8b92a5; font-size: 11px; padding: 2px 0;");
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("Prompt name..."));
    m_nameEdit->setMinimumHeight(30);
    right->addWidget(nameLabel);
    right->addWidget(m_nameEdit);

    auto* promptLabel = new QLabel(tr("System Prompt:"));
    promptLabel->setStyleSheet("font-weight: 600; color: #8b92a5; font-size: 11px; padding: 6px 0 2px 0;");
    m_promptEdit = new QPlainTextEdit;
    m_promptEdit->setPlaceholderText(tr(
        "You are a professional translator specializing in...\n"
        "Translate the following text from %1 to %2.\n"
        "Requirements:\n"
        "- Keep technical terms in English\n"
        "- ..."));
    m_promptEdit->setMinimumHeight(220);
    m_promptEdit->setTabStopDistance(20);
    right->addWidget(promptLabel);
    right->addWidget(m_promptEdit, 1);

    // Hint
    auto* hint = new QLabel(tr("Use %1 for source language and %2 for target language."));
    hint->setStyleSheet("color: #6b6f7b; font-size: 11px; padding: 4px 0;");
    right->addWidget(hint);

    // Action buttons
    auto* btnRow = new QHBoxLayout;
    m_addBtn = new QPushButton(tr("Add"));
    m_dupBtn = new QPushButton(tr("Duplicate"));
    m_deleteBtn = new QPushButton(tr("Delete"));
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_dupBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addStretch();
    right->addLayout(btnRow);

    // --- Splitter ---
    auto* splitter = new QSplitter(Qt::Horizontal);
    auto* leftWidget = new QWidget;
    leftWidget->setLayout(leftPanel);
    splitter->addWidget(leftWidget);
    auto* rightWidget = new QWidget;
    rightWidget->setLayout(right);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 5);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter);

    // Bottom bar: Set Active | Save | [spacer] | Close | Cancel
    auto* bottomRow = new QHBoxLayout;
    auto* setActiveBtn = new QPushButton(tr("Set as Active"));
    setActiveBtn->setToolTip(tr("Make the selected prompt the active translator persona"));
    connect(setActiveBtn, &QPushButton::clicked, this, [this]() {
        int row = currentListRow();
        if (row >= 0 && row < m_presets.size()) {
            saveCurrentEdits();
            m_activeId = m_presets[row].id;
            refreshList();
        }
    });
    bottomRow->addWidget(setActiveBtn);

    m_saveBtn = new QPushButton(tr("Save"));
    m_saveBtn->setObjectName("applyBtn");
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() {
        saveCurrentEdits();
    });
    bottomRow->addWidget(m_saveBtn);

    bottomRow->addStretch();
    m_okBtn     = new QPushButton(tr("Close"));
    m_cancelBtn = new QPushButton(tr("Cancel"));
    bottomRow->addWidget(m_okBtn);
    bottomRow->addWidget(m_cancelBtn);
    mainLayout->addLayout(bottomRow);

    // Connect
    connect(m_list, &QListWidget::currentRowChanged,
            this, &PromptManagerDialog::onSelectionChanged);
    connect(m_addBtn,    &QPushButton::clicked, this, &PromptManagerDialog::onAdd);
    connect(m_dupBtn,    &QPushButton::clicked, this, &PromptManagerDialog::onDuplicate);
    connect(m_deleteBtn, &QPushButton::clicked, this, &PromptManagerDialog::onDelete);
    connect(m_okBtn,     &QPushButton::clicked, this, [this]() {
        saveCurrentEdits();  // save before closing
        accept();
    });
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // Initialise editors as disabled until a row is selected
    m_nameEdit->setEnabled(false);
    m_promptEdit->setEnabled(false);
    m_deleteBtn->setEnabled(false);

    refreshList();
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);  // triggers onSelectionChanged → loads first item
}

QVector<PromptPreset> PromptManagerDialog::presets() const { return m_presets; }
QString PromptManagerDialog::activeId() const { return m_activeId; }

void PromptManagerDialog::refreshList() {
    int oldRow = m_list->currentRow();
    m_list->blockSignals(true);
    m_list->clear();
    for (const auto& p : m_presets) {
        QString label = p.name;
        if (p.id == m_activeId)
            label = QString::fromUtf8("\xe2\x98\x85  ") + label;  // ★ active star
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, p.id);
        if (p.isBuiltIn) {
            item->setForeground(QColor("#818cf8"));
        }
        if (p.id == m_activeId) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
        m_list->addItem(item);
    }
    m_list->blockSignals(false);
    // Restore selection if possible
    if (oldRow >= 0 && oldRow < m_list->count())
        m_list->setCurrentRow(oldRow);
}

int PromptManagerDialog::currentListRow() const {
    return m_list->currentRow();
}

void PromptManagerDialog::onSelectionChanged() {
    int newRow = currentListRow();
    // Save edits made to the previously-selected item
    saveCurrentEdits();

    if (newRow < 0 || newRow >= m_presets.size()) {
        m_nameEdit->clear();
        m_promptEdit->clear();
        m_nameEdit->setEnabled(false);
        m_promptEdit->setEnabled(false);
        m_deleteBtn->setEnabled(false);
        return;
    }
    m_nameEdit->setEnabled(true);
    m_promptEdit->setEnabled(true);
    const auto& p = m_presets[newRow];
    m_nameEdit->setText(p.name);
    m_promptEdit->setPlainText(p.prompt);
    m_deleteBtn->setEnabled(!p.isBuiltIn);
    m_currentEditRow = newRow;
}

void PromptManagerDialog::saveCurrentEdits() {
    if (m_currentEditRow < 0 || m_currentEditRow >= m_presets.size()) return;
    m_presets[m_currentEditRow].name = m_nameEdit->text().trimmed();
    QString prompt = m_promptEdit->toPlainText().trimmed();

    // Auto-append essential rules if missing
    if (!prompt.contains("%1") || !prompt.contains("%2")) {
        if (!prompt.isEmpty()) prompt += "\n\n";
        prompt += "Translate the following text from %1 to %2.";
    }
    QString bareRule = "Only translate. Never explain code, never add commentary.";
    if (!prompt.contains("Never explain")) {
        prompt += "\n" + bareRule;
    }

    m_presets[m_currentEditRow].prompt = prompt;

    // Update list item text in-place
    auto* item = m_list->item(m_currentEditRow);
    if (item) {
        QString label = m_presets[m_currentEditRow].name;
        if (m_presets[m_currentEditRow].id == m_activeId)
            label = QString::fromUtf8("\xe2\x98\x85  ") + label;
        item->setText(label);
    }
}

void PromptManagerDialog::onAdd() {
    int oldRow = currentListRow();
    saveCurrentEdits();

    PromptPreset p;
    p.id        = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    p.name      = tr("New Prompt");
    p.prompt    = QStringLiteral(
        "You are a professional translator.\n"
        "Translate the following text from %1 to %2.\n"
        "Keep the translation accurate and natural.");
    p.isBuiltIn = false;
    m_presets.append(p);
    refreshList();
    m_list->setCurrentRow(m_presets.size() - 1);
}

void PromptManagerDialog::onDelete() {
    int row = currentListRow();
    if (row < 0 || row >= m_presets.size()) return;
    if (m_presets[row].isBuiltIn) return;

    if (QMessageBox::question(this, tr("Delete Prompt"),
            tr("Delete \"%1\"?").arg(m_presets[row].name))
        != QMessageBox::Yes) return;

    m_presets.removeAt(row);
    refreshList();
    if (m_list->count() > 0)
        m_list->setCurrentRow(qMin(row, m_presets.size() - 1));
    else {
        m_nameEdit->clear();
        m_promptEdit->clear();
        m_nameEdit->setEnabled(false);
        m_promptEdit->setEnabled(false);
        m_deleteBtn->setEnabled(false);
    }
}

void PromptManagerDialog::onDuplicate() {
    int row = currentListRow();
    saveCurrentEdits();
    if (row < 0 || row >= m_presets.size()) return;

    PromptPreset p = m_presets[row];
    p.id        = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    p.name      = p.name + " " + tr("(Copy)");
    p.isBuiltIn = false;
    m_presets.append(p);
    refreshList();
    m_list->setCurrentRow(m_presets.size() - 1);
}
