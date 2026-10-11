/**
 * @licence app begin@
 * Copyright (C) 2026  COVESA
 *
 * This file is part of COVESA Project Dlt Viewer.
 *
 * \copyright
 * This Source Code Form is subject to the terms of the
 * Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with
 * this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * \file diagrambuilderdialog.cpp
 * @licence end@
 */

#include "diagrambuilderdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QColorDialog>
#include <QMessageBox>

namespace DltDiagram {

DiagramBuilderDialog::DiagramBuilderDialog(const QStringList &existingTabs,
                                           const QString &currentTab,
                                           const QColor &currentTabColor,
                                           DiagramType defaultType,
                                           QWidget *parent)
    : QDialog(parent)
    , m_tabColor(currentTabColor)
{
    setWindowTitle(QStringLiteral("DLT Diagram & Transition Log Matching Builder"));
    resize(980, 720);
    setupUi(existingTabs, currentTab, defaultType);
}

void DiagramBuilderDialog::setupUi(const QStringList &existingTabs,
                                   const QString &currentTab,
                                   DiagramType defaultType)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // --- 1. Basic Metadata & Tab Color Group ---
    QGroupBox *metaGroup = new QGroupBox(QStringLiteral("1. Tab (JSON File), Tab Color & Diagram Basic Settings"), this);
    QFormLayout *metaForm = new QFormLayout(metaGroup);

    QHBoxLayout *tabTypeRow = new QHBoxLayout();
    m_tabCombo = new QComboBox(metaGroup);
    m_tabCombo->setEditable(true);
    m_tabCombo->addItems(existingTabs.isEmpty() ? QStringList{QStringLiteral("Default")} : existingTabs);
    if (!currentTab.isEmpty()) {
        m_tabCombo->setCurrentText(currentTab);
    }
    m_tabCombo->setMinimumWidth(160);
    m_tabCombo->setToolTip(QStringLiteral("Select an existing Tab or type a new Tab name (creates <TabName>.json automatically)."));

    m_tabColorBtn = new QPushButton(QStringLiteral("Tab Color..."), metaGroup);
    m_tabColorBtn->setToolTip(QStringLiteral("Choose a highlight color for this Tab on the Tab Bar"));
    connect(m_tabColorBtn, &QPushButton::clicked, this, &DiagramBuilderDialog::onPickTabColorClicked);

    m_resetTabColorBtn = new QPushButton(QStringLiteral("Reset"), metaGroup);
    m_resetTabColorBtn->setToolTip(QStringLiteral("Reset Tab color to default"));
    m_resetTabColorBtn->setMaximumWidth(52);
    connect(m_resetTabColorBtn, &QPushButton::clicked, this, &DiagramBuilderDialog::onResetTabColorClicked);
    updateTabColorButtonStyle();

    m_typeCombo = new QComboBox(metaGroup);
    m_typeCombo->addItem(QStringLiteral("State Diagram (상태 다이어그램)"), static_cast<int>(DiagramType::StateDiagram));
    m_typeCombo->addItem(QStringLiteral("Sequence Diagram (시퀀스 다이어그램)"), static_cast<int>(DiagramType::SequenceDiagram));
    m_typeCombo->addItem(QStringLiteral("Flow Diagram (플로우 다이어그램)"), static_cast<int>(DiagramType::FlowDiagram));
    m_typeCombo->setCurrentIndex(static_cast<int>(defaultType));
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiagramBuilderDialog::onTypeChanged);

    m_formatCombo = new QComboBox(metaGroup);
    m_formatCombo->addItem(QStringLiteral("Mermaid"));
    m_formatCombo->addItem(QStringLiteral("PlantUML"));

    tabTypeRow->addWidget(m_tabCombo);
    tabTypeRow->addWidget(m_tabColorBtn);
    tabTypeRow->addWidget(m_resetTabColorBtn);
    tabTypeRow->addSpacing(8);
    tabTypeRow->addWidget(new QLabel(QStringLiteral("Category:"), metaGroup));
    tabTypeRow->addWidget(m_typeCombo);
    tabTypeRow->addWidget(new QLabel(QStringLiteral("Script Format:"), metaGroup));
    tabTypeRow->addWidget(m_formatCombo);
    tabTypeRow->addStretch(1);
    metaForm->addRow(QStringLiteral("Tab (<TabName>.json):"), tabTypeRow);

    QHBoxLayout *titleIdRow = new QHBoxLayout();
    m_titleEdit = new QLineEdit(metaGroup);
    m_titleEdit->setPlaceholderText(QStringLiteral("e.g. Audio Power State Machine (Sorted alphabetically A-Z)"));
    connect(m_titleEdit, &QLineEdit::textEdited, this, &DiagramBuilderDialog::onTitleEdited);

    m_idEdit = new QLineEdit(metaGroup);
    m_idEdit->setPlaceholderText(QStringLiteral("audio_power_state"));
    m_idEdit->setMaximumWidth(190);
    connect(m_idEdit, &QLineEdit::textEdited, this, [this]() { m_idManuallyEdited = true; });

    m_initialStateCombo = new QComboBox(metaGroup);
    m_initialStateCombo->setEditable(true);
    m_initialStateCombo->setMinimumWidth(150);
    m_initialStateCombo->setToolTip(QStringLiteral("Select or type the Initial State / Start Step"));

    titleIdRow->addWidget(m_titleEdit, 2);
    titleIdRow->addWidget(new QLabel(QStringLiteral("ID:"), metaGroup));
    titleIdRow->addWidget(m_idEdit, 1);
    titleIdRow->addWidget(new QLabel(QStringLiteral("Initial State/Step:"), metaGroup));
    titleIdRow->addWidget(m_initialStateCombo, 1);
    metaForm->addRow(QStringLiteral("Diagram Name:"), titleIdRow);

    mainLayout->addWidget(metaGroup);

    // --- 2. Builder Tabs ---
    QTabWidget *builderTabs = new QTabWidget(this);

    // =========================================================================
    // Tab A: Nodes & Transitions (From / To Drop-Down + Transition Name + DLT Log Matching)
    // =========================================================================
    QWidget *graphTab = new QWidget(builderTabs);
    QVBoxLayout *graphLayout = new QVBoxLayout(graphTab);

    // Nodes section
    QHBoxLayout *nodeBtnBar = new QHBoxLayout();
    nodeBtnBar->addWidget(new QLabel(
        QStringLiteral("<b>1. States / Participants / Flow Steps</b> (Used in From / To Drop-Downs below):"),
        graphTab));
    nodeBtnBar->addStretch(1);
    QPushButton *btnAddNode = new QPushButton(QStringLiteral("+ Add Node / Participant"), graphTab);
    QPushButton *btnDelNode = new QPushButton(QStringLiteral("- Remove Selected"), graphTab);
    connect(btnAddNode, &QPushButton::clicked, this, &DiagramBuilderDialog::onAddNodeClicked);
    connect(btnDelNode, &QPushButton::clicked, this, &DiagramBuilderDialog::onRemoveNodeClicked);
    nodeBtnBar->addWidget(btnAddNode);
    nodeBtnBar->addWidget(btnDelNode);
    graphLayout->addLayout(nodeBtnBar);

    m_nodesTable = new QTableWidget(0, 3, graphTab);
    m_nodesTable->setHorizontalHeaderLabels({
        QStringLiteral("Node / Participant ID (e.g. Standby)"),
        QStringLiteral("Display Label (e.g. Standby Mode)"),
        QStringLiteral("Shape (For Flow Diagram)")
    });
    m_nodesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_nodesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_nodesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_nodesTable->verticalHeader()->setVisible(false);
    connect(m_nodesTable, &QTableWidget::itemChanged,
            this, &DiagramBuilderDialog::onNodeTableItemChanged);
    graphLayout->addWidget(m_nodesTable, 2);

    // Transitions + Direct DLT Log Matching section
    QHBoxLayout *edgeBtnBar = new QHBoxLayout();
    edgeBtnBar->addWidget(new QLabel(
        QStringLiteral("<b>2. Transitions (From/To Drop-Down + Transition Name + DLT Log Matching)</b>:"),
        graphTab));
    edgeBtnBar->addStretch(1);
    QPushButton *btnAddEdge = new QPushButton(QStringLiteral("+ Add Transition"), graphTab);
    QPushButton *btnDelEdge = new QPushButton(QStringLiteral("- Remove Selected"), graphTab);
    connect(btnAddEdge, &QPushButton::clicked, this, &DiagramBuilderDialog::onAddEdgeClicked);
    connect(btnDelEdge, &QPushButton::clicked, this, &DiagramBuilderDialog::onRemoveEdgeClicked);
    edgeBtnBar->addWidget(btnAddEdge);
    edgeBtnBar->addWidget(btnDelEdge);
    graphLayout->addLayout(edgeBtnBar);

    m_edgesTable = new QTableWidget(0, 8, graphTab);
    m_edgesTable->setHorizontalHeaderLabels({
        QStringLiteral("From (Drop-Down)"),
        QStringLiteral("To (Drop-Down)"),
        QStringLiteral("Transition Name"),
        QStringLiteral("ECUID"),
        QStringLiteral("APID"),
        QStringLiteral("CTID"),
        QStringLiteral("DLT Log Matching (Keyword / Regex)"),
        QStringLiteral("Arrow Style")
    });
    m_edgesTable->setColumnWidth(0, 130);
    m_edgesTable->setColumnWidth(1, 130);
    m_edgesTable->setColumnWidth(2, 145);
    m_edgesTable->setColumnWidth(3, 55);
    m_edgesTable->setColumnWidth(4, 55);
    m_edgesTable->setColumnWidth(5, 55);
    m_edgesTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_edgesTable->setColumnWidth(7, 85);
    m_edgesTable->verticalHeader()->setVisible(false);
    graphLayout->addWidget(m_edgesTable, 3);

    builderTabs->addTab(graphTab, QStringLiteral("1. Nodes && Transitions (+ DLT Log Matching)"));

    // =========================================================================
    // Tab B: PlantUML / Mermaid Script Sync
    // =========================================================================
    QWidget *scriptTab = new QWidget(builderTabs);
    QVBoxLayout *scriptLayout = new QVBoxLayout(scriptTab);
    QHBoxLayout *scriptBtnBar = new QHBoxLayout();
    QPushButton *btnGenScript = new QPushButton(QStringLiteral("Generate Script from Tables"), scriptTab);
    QPushButton *btnParseScript = new QPushButton(QStringLiteral("Parse Script into Tables"), scriptTab);
    connect(btnGenScript, &QPushButton::clicked, this, &DiagramBuilderDialog::onGenerateScriptFromTablesClicked);
    connect(btnParseScript, &QPushButton::clicked, this, &DiagramBuilderDialog::onParseScriptToTablesClicked);
    scriptBtnBar->addWidget(btnGenScript);
    scriptBtnBar->addWidget(btnParseScript);
    scriptBtnBar->addStretch(1);

    m_scriptEdit = new QPlainTextEdit(scriptTab);
    m_scriptEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_scriptEdit->setPlaceholderText(
        QStringLiteral("Optional: Paste Mermaid (stateDiagram-v2 / sequenceDiagram / flowchart TD) or PlantUML (@startuml ... @enduml) here."));

    scriptLayout->addLayout(scriptBtnBar);
    scriptLayout->addWidget(m_scriptEdit, 1);
    builderTabs->addTab(scriptTab, QStringLiteral("2. PlantUML / Mermaid Script"));

    mainLayout->addWidget(builderTabs, 1);

    // --- 3. Live DLT Log Matching Tester ---
    QGroupBox *testerBox = new QGroupBox(QStringLiteral("Live DLT Log Matching Tester (Transition 로그 매칭 검증기)"), this);
    QVBoxLayout *testerLayout = new QVBoxLayout(testerBox);
    QHBoxLayout *testerRow = new QHBoxLayout();
    m_samplePayloadEdit = new QLineEdit(testerBox);
    m_samplePayloadEdit->setPlaceholderText(
        QStringLiteral("샘플 DLT 로그 Payload를 입력해 매칭을 테스트하세요 (예: State changed: Standby -> Active)"));
    QPushButton *btnTestRegex = new QPushButton(QStringLiteral("Test DLT Log Matching"), testerBox);
    connect(btnTestRegex, &QPushButton::clicked, this, &DiagramBuilderDialog::onTestRegexClicked);
    testerRow->addWidget(m_samplePayloadEdit, 1);
    testerRow->addWidget(btnTestRegex);
    m_regexTestResultLabel = new QLabel(
        QStringLiteral("위 입력창에 DLT 로그 Payload를 입력하고 'Test DLT Log Matching'을 클릭하면 어떤 Transition이 동작하는지 즉시 확인됩니다."),
        testerBox);
    m_regexTestResultLabel->setStyleSheet(QStringLiteral("color: #475569;"));
    testerLayout->addLayout(testerRow);
    testerLayout->addWidget(m_regexTestResultLabel);
    mainLayout->addWidget(testerBox);

    // --- 4. Error Banner & Dialog Buttons ---
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #dc2626; font-weight: bold;"));
    mainLayout->addWidget(m_errorLabel);

    QDialogButtonBox *btnBox = new QDialogButtonBox(this);
    QPushButton *btnSave = btnBox->addButton(
        QStringLiteral("Save to <Tab>.json && Hot-Reload Now"), QDialogButtonBox::AcceptRole);
    btnSave->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #059669; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px; }"));
    btnBox->addButton(QDialogButtonBox::Cancel);
    connect(btnBox, &QDialogButtonBox::accepted, this, &DiagramBuilderDialog::onAcceptClicked);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);
}

void DiagramBuilderDialog::updateTabColorButtonStyle()
{
    if (!m_tabColorBtn) {
        return;
    }
    if (m_tabColor.isValid()) {
        const QColor textCol = (m_tabColor.lightness() < 140) ? QColor(Qt::white) : QColor(Qt::black);
        m_tabColorBtn->setText(QStringLiteral("Tab Color (%1)").arg(m_tabColor.name(QColor::HexRgb)));
        m_tabColorBtn->setStyleSheet(
            QStringLiteral("QPushButton { background-color: %1; color: %2; font-weight: bold; padding: 3px 8px; border-radius: 3px; }")
                .arg(m_tabColor.name(QColor::HexRgb), textCol.name(QColor::HexRgb)));
    } else {
        m_tabColorBtn->setText(QStringLiteral("Tab Color..."));
        m_tabColorBtn->setStyleSheet(QString());
    }
}

void DiagramBuilderDialog::onPickTabColorClicked()
{
    const QColor initial = m_tabColor.isValid() ? m_tabColor : QColor(37, 99, 235);
    const QColor picked = QColorDialog::getColor(initial, this, QStringLiteral("Select Tab Color"));
    if (picked.isValid()) {
        m_tabColor = picked;
        updateTabColorButtonStyle();
    }
}

void DiagramBuilderDialog::onResetTabColorClicked()
{
    m_tabColor = QColor();
    updateTabColorButtonStyle();
}

QStringList DiagramBuilderDialog::currentNodeIds() const
{
    QStringList ids;
    if (!m_nodesTable) {
        return ids;
    }
    for (int r = 0; r < m_nodesTable->rowCount(); ++r) {
        if (const QTableWidgetItem *item = m_nodesTable->item(r, 0)) {
            const QString id = item->text().trimmed();
            if (!id.isEmpty() && !ids.contains(id)) {
                ids.append(id);
            }
        }
    }
    return ids;
}

void DiagramBuilderDialog::updateComboItemsPreservingText(QComboBox *combo,
                                                          const QStringList &items,
                                                          const QString &preferredText)
{
    if (!combo) {
        return;
    }
    const QString currentText = preferredText.isEmpty() ? combo->currentText().trimmed() : preferredText.trimmed();

    combo->blockSignals(true);
    combo->clear();

    QStringList merged = items;
    if (!currentText.isEmpty() && !merged.contains(currentText)) {
        merged.prepend(currentText);
    }
    combo->addItems(merged);
    combo->setCurrentText(currentText);
    combo->blockSignals(false);
}

void DiagramBuilderDialog::refreshNodeDropDowns()
{
    if (m_updatingTables) {
        return;
    }

    const QStringList nodeIds = currentNodeIds();
    QStringList fromNodeOptions = nodeIds;
    if (!fromNodeOptions.contains(QStringLiteral("*"))) {
        fromNodeOptions.append(QStringLiteral("*"));
    }

    // 1. Update Initial State / Start Step drop-down
    updateComboItemsPreservingText(m_initialStateCombo, nodeIds);

    // 2. Update Transitions Table From (col 0) & To (col 1) drop-downs
    if (m_edgesTable) {
        for (int r = 0; r < m_edgesTable->rowCount(); ++r) {
            if (QComboBox *fromCombo = qobject_cast<QComboBox *>(m_edgesTable->cellWidget(r, 0))) {
                updateComboItemsPreservingText(fromCombo, fromNodeOptions);
            }
            if (QComboBox *toCombo = qobject_cast<QComboBox *>(m_edgesTable->cellWidget(r, 1))) {
                updateComboItemsPreservingText(toCombo, nodeIds);
            }
        }
    }
}

void DiagramBuilderDialog::onNodeTableItemChanged(QTableWidgetItem *item)
{
    if (!item || m_updatingTables) {
        return;
    }
    if (item->column() == 0) {
        refreshNodeDropDowns();
    }
}

void DiagramBuilderDialog::onTitleEdited(const QString &text)
{
    if (!m_idManuallyEdited && m_idEdit) {
        QString slug = text.trimmed().toLower();
        slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
        slug.remove(QRegularExpression(QStringLiteral("^_|_$")));
        m_idEdit->setText(slug);
    }
}

void DiagramBuilderDialog::onTypeChanged(int index)
{
    Q_UNUSED(index);
}

void DiagramBuilderDialog::appendNodeRow(const QString &id, const QString &label, DiagramNodeShape shape)
{
    const bool prevUpdating = m_updatingTables;
    m_updatingTables = true;

    const int row = m_nodesTable->rowCount();
    m_nodesTable->insertRow(row);
    m_nodesTable->setItem(row, 0, new QTableWidgetItem(id));
    m_nodesTable->setItem(row, 1, new QTableWidgetItem(label.isEmpty() ? id : label));

    QComboBox *shapeCombo = new QComboBox(m_nodesTable);
    shapeCombo->addItem(QStringLiteral("Standard [Step / State]"), static_cast<int>(DiagramNodeShape::Standard));
    shapeCombo->addItem(QStringLiteral("Start / End ([Pill])"), static_cast<int>(DiagramNodeShape::StartEnd));
    shapeCombo->addItem(QStringLiteral("Decision {Diamond?}"), static_cast<int>(DiagramNodeShape::Decision));
    shapeCombo->addItem(QStringLiteral("Subroutine [[Process]]"), static_cast<int>(DiagramNodeShape::Subroutine));
    shapeCombo->setCurrentIndex(static_cast<int>(shape));
    m_nodesTable->setCellWidget(row, 2, shapeCombo);

    m_updatingTables = prevUpdating;
    refreshNodeDropDowns();
}

void DiagramBuilderDialog::appendEdgeRow(const DiagramEdge &edge)
{
    const int row = m_edgesTable->rowCount();
    m_edgesTable->insertRow(row);

    const QStringList nodeIds = currentNodeIds();
    QStringList fromOptions = nodeIds;
    if (!fromOptions.contains(QStringLiteral("*"))) {
        fromOptions.append(QStringLiteral("*"));
    }

    // Col 0: From Drop-Down (QComboBox)
    QComboBox *fromCombo = new QComboBox(m_edgesTable);
    fromCombo->setEditable(true);
    fromCombo->setToolTip(QStringLiteral("Select 'From' State / Participant / Step from drop-down (or '*' for any state)"));
    updateComboItemsPreservingText(fromCombo, fromOptions, edge.from);
    m_edgesTable->setCellWidget(row, 0, fromCombo);

    // Col 1: To Drop-Down (QComboBox)
    QComboBox *toCombo = new QComboBox(m_edgesTable);
    toCombo->setEditable(true);
    toCombo->setToolTip(QStringLiteral("Select 'To' State / Participant / Step from drop-down"));
    updateComboItemsPreservingText(toCombo, nodeIds, edge.to);
    m_edgesTable->setCellWidget(row, 1, toCombo);

    // Col 2: Transition Name
    QTableWidgetItem *nameItem = new QTableWidgetItem(edge.label);
    nameItem->setToolTip(QStringLiteral("Transition Name displayed on the arrow (e.g. EvPowerOn, PlayRequest)"));
    m_edgesTable->setItem(row, 2, nameItem);

    // Col 3: ECUID
    QTableWidgetItem *ecuItem = new QTableWidgetItem(edge.ecuid);
    ecuItem->setToolTip(QStringLiteral("Optional DLT ECU ID filter (leave blank to match any ECU)"));
    m_edgesTable->setItem(row, 3, ecuItem);

    // Col 4: APID
    QTableWidgetItem *apidItem = new QTableWidgetItem(edge.apid);
    apidItem->setToolTip(QStringLiteral("Optional DLT Application ID filter (e.g. AUD, SYS)"));
    m_edgesTable->setItem(row, 4, apidItem);

    // Col 5: CTID
    QTableWidgetItem *ctidItem = new QTableWidgetItem(edge.ctid);
    ctidItem->setToolTip(QStringLiteral("Optional DLT Context ID filter (e.g. PWR, MGR)"));
    m_edgesTable->setItem(row, 5, ctidItem);

    // Col 6: DLT Log Matching (Keyword / Regex)
    QTableWidgetItem *matchItem = new QTableWidgetItem(edge.logMatch);
    matchItem->setToolTip(QStringLiteral("DLT Payload keyword or regular expression that triggers this Transition (e.g. PowerOn or State:\\s*Standby\\s*->\\s*Active)"));
    m_edgesTable->setItem(row, 6, matchItem);

    // Col 7: Arrow Style
    QComboBox *styleCombo = new QComboBox(m_edgesTable);
    styleCombo->addItems({QStringLiteral("sync"), QStringLiteral("async"), QStringLiteral("reply")});
    styleCombo->setCurrentText(edge.arrowStyle.isEmpty() ? QStringLiteral("sync") : edge.arrowStyle);
    m_edgesTable->setCellWidget(row, 7, styleCombo);
}

void DiagramBuilderDialog::onAddNodeClicked()
{
    const int n = m_nodesTable->rowCount() + 1;
    const QString newId = QStringLiteral("State%1").arg(n);
    appendNodeRow(newId, newId, DiagramNodeShape::Standard);
    if (m_initialStateCombo && m_initialStateCombo->currentText().trimmed().isEmpty()) {
        m_initialStateCombo->setCurrentText(newId);
    }
}

void DiagramBuilderDialog::onRemoveNodeClicked()
{
    const int row = m_nodesTable->currentRow();
    if (row >= 0) {
        m_nodesTable->removeRow(row);
        refreshNodeDropDowns();
    }
}

void DiagramBuilderDialog::onAddEdgeClicked()
{
    const QStringList ids = currentNodeIds();
    DiagramEdge edge;
    edge.from = !ids.isEmpty() ? ids.first() : QStringLiteral("State1");
    edge.to = (ids.size() >= 2) ? ids.at(1) : edge.from;
    edge.label = QStringLiteral("Transition_%1").arg(m_edgesTable->rowCount() + 1);
    edge.arrowStyle = QStringLiteral("sync");
    appendEdgeRow(edge);
}

void DiagramBuilderDialog::onRemoveEdgeClicked()
{
    const int row = m_edgesTable->currentRow();
    if (row >= 0) {
        m_edgesTable->removeRow(row);
    }
}

void DiagramBuilderDialog::onTestRegexClicked()
{
    const QString sample = m_samplePayloadEdit->text();
    if (sample.isEmpty()) {
        m_regexTestResultLabel->setStyleSheet(QStringLiteral("color: #d97706; font-weight: bold;"));
        m_regexTestResultLabel->setText(QStringLiteral("먼저 테스트할 DLT 로그 Payload 문자열을 입력해 주세요."));
        return;
    }

    QString err;
    const DiagramDefinition def = collectFromUi(err);
    for (const DiagramRule &rule : def.rules) {
        if (!rule.payloadRegex.isValid() || rule.payloadRegexString.isEmpty()) {
            continue;
        }
        const QRegularExpressionMatch m = rule.payloadRegex.match(sample);
        if (m.hasMatch()) {
            QString from = rule.fromTemplate;
            QString to = rule.toTemplate;
            QString lbl = rule.labelTemplate;
            for (int g = 9; g >= 1; --g) {
                const QString tok = QStringLiteral("$%1").arg(g);
                const QString cap = (g <= m.lastCapturedIndex()) ? m.captured(g) : QString();
                from.replace(tok, cap);
                to.replace(tok, cap);
                lbl.replace(tok, cap);
            }
            m_regexTestResultLabel->setStyleSheet(QStringLiteral("color: #047857; font-weight: bold;"));
            m_regexTestResultLabel->setText(
                QStringLiteral("MATCHED '%1'  ==>  From: [%2]  ->  To: [%3]  |  Transition Name: [%4]")
                    .arg(rule.name, from, to, lbl));
            return;
        }
    }

    m_regexTestResultLabel->setStyleSheet(QStringLiteral("color: #dc2626; font-weight: bold;"));
    m_regexTestResultLabel->setText(QStringLiteral("일치하는 Transition DLT Log Matching이 없습니다."));
}

void DiagramBuilderDialog::onGenerateScriptFromTablesClicked()
{
    QString err;
    DiagramDefinition def = collectFromUi(err);
    DiagramSnapshot emptySnap;
    emptySnap.currentState = def.initialState;
    const QString script = (def.format == DiagramFormat::PlantUML)
                               ? DiagramExporter::generatePlantUml(def, {}, emptySnap)
                               : DiagramExporter::generateMermaid(def, {}, emptySnap);
    m_scriptEdit->setPlainText(script);
}

void DiagramBuilderDialog::onParseScriptToTablesClicked()
{
    const QString src = m_scriptEdit->toPlainText();
    if (src.trimmed().isEmpty()) {
        return;
    }

    QString err;
    DiagramDefinition def = collectFromUi(err);
    def.nodes.clear();
    def.edges.clear();
    def.diagramSource = src;
    DiagramParser::parseDiagramSource(src, def);
    populateTablesFromDefinition(def);
}

void DiagramBuilderDialog::loadDefinition(const DiagramDefinition &def)
{
    m_idManuallyEdited = true;
    m_tabCombo->setCurrentText(def.tabName);
    m_typeCombo->setCurrentIndex(static_cast<int>(def.type));
    m_formatCombo->setCurrentIndex(def.format == DiagramFormat::PlantUML ? 1 : 0);
    m_titleEdit->setText(def.title);
    m_idEdit->setText(def.id);
    m_scriptEdit->setPlainText(def.diagramSource);

    populateTablesFromDefinition(def);
    m_initialStateCombo->setCurrentText(def.initialState);
}

void DiagramBuilderDialog::populateTablesFromDefinition(const DiagramDefinition &def)
{
    m_updatingTables = true;
    m_nodesTable->setRowCount(0);
    for (const DiagramNode &n : def.nodes) {
        appendNodeRow(n.id, n.label, n.shape);
    }
    m_updatingTables = false;

    refreshNodeDropDowns();
    if (!def.initialState.isEmpty()) {
        updateComboItemsPreservingText(m_initialStateCombo, currentNodeIds(), def.initialState);
    }

    m_edgesTable->setRowCount(0);
    for (const DiagramEdge &e : def.edges) {
        appendEdgeRow(e);
    }

    // Preserve any legacy dynamic rules that aren't already represented in edges
    m_preservedRules.clear();
    for (const DiagramRule &r : def.rules) {
        bool representedInEdge = false;
        for (const DiagramEdge &e : def.edges) {
            const QString edgeRegex = e.logMatch.isEmpty() ? QStringLiteral(".*") : e.logMatch;
            if ((!e.logMatch.isEmpty() || !e.ecuid.isEmpty() || !e.apid.isEmpty() || !e.ctid.isEmpty()) &&
                r.fromTemplate == e.from &&
                r.toTemplate == e.to &&
                r.payloadRegexString == edgeRegex &&
                r.ecuid == e.ecuid &&
                r.apid == e.apid &&
                r.ctid == e.ctid) {
                representedInEdge = true;
                break;
            }
        }
        if (!representedInEdge) {
            m_preservedRules.append(r);
        }
    }
}

DiagramDefinition DiagramBuilderDialog::collectFromUi(QString &errorOut) const
{
    errorOut.clear();
    DiagramDefinition def;
    def.tabName = m_tabCombo->currentText().trimmed();
    if (def.tabName.isEmpty()) {
        def.tabName = QStringLiteral("Default");
    }

    def.title = m_titleEdit->text().trimmed();
    def.id = m_idEdit->text().trimmed();
    if (def.id.isEmpty() && !def.title.isEmpty()) {
        def.id = def.title.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    }

    def.type = static_cast<DiagramType>(m_typeCombo->currentData().toInt());
    def.format = (m_formatCombo->currentIndex() == 1) ? DiagramFormat::PlantUML : DiagramFormat::Mermaid;
    def.initialState = m_initialStateCombo ? m_initialStateCombo->currentText().trimmed() : QString();
    def.diagramSource = m_scriptEdit->toPlainText().trimmed();

    if (!def.diagramSource.isEmpty() && m_nodesTable->rowCount() == 0 && m_edgesTable->rowCount() == 0) {
        DiagramParser::parseDiagramSource(def.diagramSource, def);
    }

    for (int r = 0; r < m_nodesTable->rowCount(); ++r) {
        const QString nId = m_nodesTable->item(r, 0) ? m_nodesTable->item(r, 0)->text().trimmed() : QString();
        const QString nLbl = m_nodesTable->item(r, 1) ? m_nodesTable->item(r, 1)->text().trimmed() : nId;
        DiagramNodeShape sh = DiagramNodeShape::Standard;
        if (QComboBox *cb = qobject_cast<QComboBox *>(m_nodesTable->cellWidget(r, 2))) {
            sh = static_cast<DiagramNodeShape>(cb->currentData().toInt());
        }
        if (!nId.isEmpty()) {
            def.ensureNode(nId, nLbl, nId == def.initialState, sh);
        }
    }

    // Collect Transitions (with Transition Name + Direct DLT Log Matching)
    for (int r = 0; r < m_edgesTable->rowCount(); ++r) {
        QString from;
        if (QComboBox *fromCb = qobject_cast<QComboBox *>(m_edgesTable->cellWidget(r, 0))) {
            from = fromCb->currentText().trimmed();
        } else if (m_edgesTable->item(r, 0)) {
            from = m_edgesTable->item(r, 0)->text().trimmed();
        }

        QString to;
        if (QComboBox *toCb = qobject_cast<QComboBox *>(m_edgesTable->cellWidget(r, 1))) {
            to = toCb->currentText().trimmed();
        } else if (m_edgesTable->item(r, 1)) {
            to = m_edgesTable->item(r, 1)->text().trimmed();
        }

        const QString transitionName = m_edgesTable->item(r, 2) ? m_edgesTable->item(r, 2)->text().trimmed() : QString();
        const QString ecuid = m_edgesTable->item(r, 3) ? m_edgesTable->item(r, 3)->text().trimmed() : QString();
        const QString apid = m_edgesTable->item(r, 4) ? m_edgesTable->item(r, 4)->text().trimmed() : QString();
        const QString ctid = m_edgesTable->item(r, 5) ? m_edgesTable->item(r, 5)->text().trimmed() : QString();
        const QString logMatch = m_edgesTable->item(r, 6) ? m_edgesTable->item(r, 6)->text().trimmed() : QString();

        QString style = QStringLiteral("sync");
        if (QComboBox *cb = qobject_cast<QComboBox *>(m_edgesTable->cellWidget(r, 7))) {
            style = cb->currentText();
        }

        if (!from.isEmpty() && !to.isEmpty()) {
            if (from != QStringLiteral("*")) {
                def.ensureNode(from, from, from == def.initialState);
            }
            if (to != QStringLiteral("*")) {
                def.ensureNode(to, to, to == def.initialState);
            }
            def.ensureEdge(from, to, transitionName, style, ecuid, apid, ctid, logMatch);

            // If this Transition specifies DLT Log Matching (or ECU/APID/CTID filter), create its executable DiagramRule
            if (!logMatch.isEmpty() || !ecuid.isEmpty() || !apid.isEmpty() || !ctid.isEmpty()) {
                DiagramRule trRule;
                trRule.name = transitionName.isEmpty()
                                  ? QStringLiteral("%1 -> %2").arg(from, to)
                                  : transitionName;
                trRule.ecuid = ecuid;
                trRule.apid = apid;
                trRule.ctid = ctid;
                trRule.payloadRegexString = logMatch.isEmpty() ? QStringLiteral(".*") : logMatch;
                trRule.payloadRegex = QRegularExpression(trRule.payloadRegexString);
                if (!trRule.payloadRegex.isValid()) {
                    errorOut = QStringLiteral("Invalid DLT Log Matching pattern in Transition '%1 -> %2': %3")
                                   .arg(from, to, trRule.payloadRegex.errorString());
                    return def;
                }
                trRule.fromTemplate = from;
                trRule.toTemplate = to;
                trRule.labelTemplate = transitionName.isEmpty() ? trRule.name : transitionName;
                trRule.arrowStyle = style;
                def.rules.append(trRule);
            }
        }
    }

    for (const DiagramRule &legacyRule : m_preservedRules) {
        def.rules.append(legacyRule);
    }

    if (def.title.isEmpty()) {
        errorOut = QStringLiteral("Please enter a Diagram Name.");
        return def;
    }

    if (def.initialState.isEmpty() && !def.nodes.isEmpty() && def.type != DiagramType::SequenceDiagram) {
        def.initialState = def.nodes.first().id;
    }

    return def;
}

void DiagramBuilderDialog::onAcceptClicked()
{
    QString err;
    const DiagramDefinition def = collectFromUi(err);
    if (!err.isEmpty()) {
        m_errorLabel->setText(err);
        return;
    }
    m_resultDef = def;
    accept();
}

DiagramDefinition DiagramBuilderDialog::builtDefinition() const
{
    return m_resultDef;
}

} // namespace DltDiagram
