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
 * \file diagrambuilderdialog.h
 * @licence end@
 */

#ifndef DIAGRAMBUILDERDIALOG_H
#define DIAGRAMBUILDERDIALOG_H

#include "diagrammodel.h"

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QColor>

namespace DltDiagram {

class DiagramBuilderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiagramBuilderDialog(const QStringList &existingTabs,
                                  const QString &currentTab,
                                  const QColor &currentTabColor,
                                  DiagramType defaultType,
                                  QWidget *parent = nullptr);

    void loadDefinition(const DiagramDefinition &def);
    DiagramDefinition builtDefinition() const;
    QColor selectedTabColor() const { return m_tabColor; }

private slots:
    void onTitleEdited(const QString &text);
    void onTypeChanged(int index);
    void onPickTabColorClicked();
    void onResetTabColorClicked();
    void onAddNodeClicked();
    void onRemoveNodeClicked();
    void onNodeTableItemChanged(QTableWidgetItem *item);
    void onAddEdgeClicked();
    void onRemoveEdgeClicked();
    void onTestRegexClicked();
    void onGenerateScriptFromTablesClicked();
    void onParseScriptToTablesClicked();
    void onAcceptClicked();

private:
    void setupUi(const QStringList &existingTabs, const QString &currentTab, DiagramType defaultType);
    void updateTabColorButtonStyle();
    void populateTablesFromDefinition(const DiagramDefinition &def);
    DiagramDefinition collectFromUi(QString &errorOut) const;

    QStringList currentNodeIds() const;
    void refreshNodeDropDowns();
    static void updateComboItemsPreservingText(QComboBox *combo,
                                               const QStringList &items,
                                               const QString &preferredText = QString());

    void appendNodeRow(const QString &id, const QString &label, DiagramNodeShape shape);
    void appendEdgeRow(const DiagramEdge &edge);

    bool m_idManuallyEdited = false;
    bool m_updatingTables = false;
    QColor m_tabColor;

    QComboBox *m_tabCombo = nullptr;
    QPushButton *m_tabColorBtn = nullptr;
    QPushButton *m_resetTabColorBtn = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_idEdit = nullptr;
    QComboBox *m_initialStateCombo = nullptr;

    QTableWidget *m_nodesTable = nullptr;
    QTableWidget *m_edgesTable = nullptr;
    QList<DiagramRule> m_preservedRules;

    QLineEdit *m_samplePayloadEdit = nullptr;
    QLabel *m_regexTestResultLabel = nullptr;

    QPlainTextEdit *m_scriptEdit = nullptr;
    QLabel *m_errorLabel = nullptr;

    DiagramDefinition m_resultDef;
};

} // namespace DltDiagram

#endif // DIAGRAMBUILDERDIALOG_H
