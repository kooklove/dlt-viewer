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
 * \file form.h
 * @licence end@
 */

#ifndef DLTDIAGRAM_FORM_H
#define DLTDIAGRAM_FORM_H

#include "diagrammodel.h"
#include "diagramengine.h"
#include "diagramgraphicsview.h"

#include <QWidget>
#include <QMenuBar>
#include <QAction>
#include <QTabBar>
#include <QTreeWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QTabWidget>

namespace DltDiagram {

class Form : public QWidget
{
    Q_OBJECT

public:
    explicit Form(DiagramStateEngine *engine, QWidget *parent = nullptr);
    ~Form() override = default;

    void onConfigUpdated(const QString &rawJsonText = QString());
    void onFileReloaded();
    void onLiveEventsUpdated(int latestDltIndex);
    void onSelectedLogChanged(int selectedDltIndex);

    int currentDiagramIndex() const;
    QString currentTabName() const;
    bool isFavoritesTabSelected() const;
    bool isTimeTravelMode() const;

signals:
    void jumpToLogRequested(int dltIndex);
    void applyJsonConfigRequested(const QString &jsonText);
    void rescanFileRequested();
    void loadConfigFileRequested(const QString &filePath);
    void saveConfigFileRequested(const QString &filePath);

private slots:
    // Files Menu
    void onShowFilesDialogClicked();
    void onHotReloadAllClicked();

    // Tabs Menu & Tab Bar
    void onTabBarCurrentChanged(int index);
    void onTabBarMoved(int from, int to);
    void onTabBarContextMenuRequested(const QPoint &pos);
    void onAddTabClicked();
    void onEditTabClicked();
    void onDeleteTabClicked();
    void onGeneralTabOrderingClicked();
    void onToggleCurrentTabFavoriteClicked();

    // State / Sequence / Flow Diagram Menus
    void onAddStateDiagramClicked();
    void onEditStateDiagramClicked();
    void onDeleteStateDiagramClicked();

    void onAddSequenceDiagramClicked();
    void onEditSequenceDiagramClicked();
    void onDeleteSequenceDiagramClicked();

    void onAddFlowDiagramClicked();
    void onEditFlowDiagramClicked();
    void onDeleteFlowDiagramClicked();

    // Favorite Menu & Diagram List Favorite Add/Remove + Keyboard Navigation
    void onShowFavoriteListDialogClicked();
    void onCategoryTreeCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onCategoryTreeItemClicked(QTreeWidgetItem *item, int column);
    void onCategoryTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onCategoryTreeContextMenuRequested(const QPoint &pos);

    // Sync, Timeline, Script & JSON slots
    void onSyncModeChanged(int index);
    void onExportFormatChanged(int index);
    void onTimelineRowClicked(int row, int column);
    void onTimelineCurrentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);
    void onApplyScriptClicked();
    void onCopyScriptClicked();
    void onApplyJsonClicked();
    void onLoadSampleJsonClicked();
    void onOpenJsonFileClicked();
    void onSaveJsonFileClicked();
    void onChooseWorkspaceFolderClicked();

private:
    void setupUi();
    void setupMenus();
    void refreshTabBar();
    void refreshCategoryTree();
    void refreshTabJsonEditor();
    void rebuildEventTimelineTable();
    void refreshDiagramAndCodeView();
    void updateTabFavoriteActionState();
    void updateStatusBanner(const DiagramDefinition &def,
                            const QVector<DiagramEvent> &events,
                            const DiagramSnapshot &snapshot);

    void openBuilderDialogForType(DiagramType type, const DiagramDefinition *existingDef = nullptr);
    void editDiagramByType(DiagramType type);
    void deleteDiagramByType(DiagramType type);
    int resolveTargetDiagramForType(DiagramType type, const QString &actionTitle);

    DiagramStateEngine *m_engine = nullptr;
    int m_currentDiagramIdx = 0;
    int m_selectedDltIndex = -1;
    int m_latestDltIndex = -1;
    bool m_updatingUi = false;
    bool m_preferFavoriteGroupInTree = false;

    // Top Menu Bar (Files, Tabs, State Diagram, Sequence Diagram, Flow Diagram, Favorite)
    QWidget *m_menuBarWidget = nullptr;
    QAction *m_actShowFiles = nullptr;
    QAction *m_actTabFavoriteToggle = nullptr;
    QComboBox *m_syncModeCombo = nullptr;
    QLabel *m_currentStateBadge = nullptr;
    QLabel *m_eventCountLabel = nullptr;

    // Clean Multi-Tab Bar (Favorite Tabs first + dedicated ★ Favorite tab)
    QTabBar *m_tabBar = nullptr;

    // Left Pane: Diagram List (with inline ★/☆ Favorite Add/Remove column)
    QTreeWidget *m_categoryTree = nullptr;

    // Center Pane: Interactive Diagram Canvas (with internal 🔍+ / 🔍- / AutoFit)
    DiagramGraphicsView *m_graphicsView = nullptr;

    // Collapsible / Toggleable Files & Inspector Panel (shown via Files -> Show Files)
    QWidget *m_filesPanel = nullptr;
    QTabWidget *m_rightTabs = nullptr;
    QTableWidget *m_timelineTable = nullptr;
    QComboBox *m_codeFormatCombo = nullptr;
    QPlainTextEdit *m_codeEditor = nullptr;
    QLabel *m_codeStatusLabel = nullptr;
    QPlainTextEdit *m_jsonEditor = nullptr;
    QLabel *m_jsonStatusLabel = nullptr;
};

} // namespace DltDiagram

#endif // DLTDIAGRAM_FORM_H
