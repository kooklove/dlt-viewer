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
 * \file form.cpp
 * @licence end@
 */

#include "form.h"
#include "diagrambuilderdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QFileDialog>
#include <QInputDialog>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QMenu>
#include <QToolButton>
#include <QFrame>
#include <QPainter>
#include <QPixmap>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QFontDatabase>

namespace DltDiagram {

namespace {
constexpr int kRoleDiagramIndex = Qt::UserRole;
constexpr int kRoleFavoriteKey = Qt::UserRole + 1;
const QString kVirtualFavoritesTabId = QStringLiteral("__FAVORITE_DIAGRAMS_TAB__");

QIcon createColorSwatchIcon(const QColor &color)
{
    if (!color.isValid()) {
        return QIcon();
    }
    QPixmap pix(12, 12);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color.darker(135), 1.0));
    p.setBrush(color);
    p.drawRoundedRect(QRectF(1.0, 1.0, 10.0, 10.0), 3.0, 3.0);
    return QIcon(pix);
}
} // namespace

Form::Form(DiagramStateEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
{
    setupUi();
    onConfigUpdated();
}

void Form::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 0, 2, 2);
    mainLayout->setSpacing(2);

    // =========================================================================
    // 1. Top Menu Bar (Right below Title Bar, high-contrast File-style menu bar)
    // =========================================================================
    setupMenus();
    mainLayout->addWidget(m_menuBarWidget);

    // =========================================================================
    // 2. Clean Tab Bar Row ('★ Favorite' Tab FIRST on the left + Favorite Tabs + General Tabs)
    //    - No 'Tabs (.json):' label, no Tab Move Left/Right buttons
    // =========================================================================
    QHBoxLayout *tabBarRow = new QHBoxLayout();
    tabBarRow->setContentsMargins(2, 1, 4, 1);
    tabBarRow->setSpacing(6);

    m_tabBar = new QTabBar(this);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabBar, &QTabBar::currentChanged, this, &Form::onTabBarCurrentChanged);
    connect(m_tabBar, &QTabBar::tabMoved, this, &Form::onTabBarMoved);
    connect(m_tabBar, &QTabBar::customContextMenuRequested, this, &Form::onTabBarContextMenuRequested);

    m_syncModeCombo = new QComboBox(this);
    m_syncModeCombo->addItem(QStringLiteral("Time-Travel"));
    m_syncModeCombo->addItem(QStringLiteral("Live"));
    m_syncModeCombo->setFixedHeight(22);
    m_syncModeCombo->setToolTip(QStringLiteral("Time-Travel (Selected Log) / Live (Latest Log)"));
    connect(m_syncModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Form::onSyncModeChanged);

    m_currentStateBadge = new QLabel(QStringLiteral("State: -"), this);
    m_currentStateBadge->setStyleSheet(QStringLiteral(
        "QLabel { background-color: #10b981; color: #ffffff; font-weight: bold; "
        "padding: 2px 8px; border-radius: 3px; font-size: 11px; }"));

    m_eventCountLabel = new QLabel(QStringLiteral("Events: 0"), this);
    m_eventCountLabel->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; padding: 0 4px;"));

    tabBarRow->addWidget(m_tabBar, 1);
    tabBarRow->addWidget(m_syncModeCombo);
    tabBarRow->addWidget(m_currentStateBadge);
    tabBarRow->addWidget(m_eventCountLabel);
    mainLayout->addLayout(tabBarRow);

    // =========================================================================
    // 3. Main Splitter:
    //    - Left: Diagram List (with ★/☆ Favorite Add/Remove column)
    //    - Center: Interactive Diagram Canvas (with internal 🔍+/🔍-/AutoFit)
    //    - Right (Hidden by default): Files / JSON / Timeline panel (shown via Files -> Show Files)
    // =========================================================================
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // Left: Diagram List (2 columns: Diagram Name | ★ Favorite Toggle)
    m_categoryTree = new QTreeWidget(splitter);
    m_categoryTree->setColumnCount(2);
    m_categoryTree->setHeaderLabels({QStringLiteral("Diagram List"), QStringLiteral("★")});
    m_categoryTree->header()->setStretchLastSection(false);
    m_categoryTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_categoryTree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_categoryTree->setColumnWidth(1, 32);
    m_categoryTree->setMinimumWidth(200);
    m_categoryTree->setIndentation(14);
    m_categoryTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_categoryTree, &QTreeWidget::currentItemChanged,
            this, &Form::onCategoryTreeCurrentItemChanged);
    connect(m_categoryTree, &QTreeWidget::itemClicked,
            this, &Form::onCategoryTreeItemClicked);
    connect(m_categoryTree, &QTreeWidget::itemDoubleClicked,
            this, &Form::onCategoryTreeItemDoubleClicked);
    connect(m_categoryTree, &QTreeWidget::customContextMenuRequested,
            this, &Form::onCategoryTreeContextMenuRequested);

    // Center: Diagram Canvas (Magnifier zoom & AutoFit are inside DiagramGraphicsView)
    m_graphicsView = new DiagramGraphicsView(splitter);
    connect(m_graphicsView, &DiagramGraphicsView::jumpToLogRequested,
            this, &Form::jumpToLogRequested);

    // Right: Files & Inspector Panel (Hidden by default; toggled via Files -> Show Files)
    m_filesPanel = new QWidget(splitter);
    QVBoxLayout *filesPanelLayout = new QVBoxLayout(m_filesPanel);
    filesPanelLayout->setContentsMargins(0, 0, 0, 0);
    filesPanelLayout->setSpacing(2);

    m_rightTabs = new QTabWidget(m_filesPanel);

    // Tab 1: JSON File Editor (<TabName>.json)
    QWidget *jsonTab = new QWidget(m_rightTabs);
    QVBoxLayout *jsonLayout = new QVBoxLayout(jsonTab);
    jsonLayout->setContentsMargins(4, 4, 4, 4);

    QHBoxLayout *jsonTopBar = new QHBoxLayout();
    QPushButton *btnApplyJson = new QPushButton(QStringLiteral("Save && Reload"), jsonTab);
    connect(btnApplyJson, &QPushButton::clicked, this, &Form::onApplyJsonClicked);

    QPushButton *btnFolder = new QPushButton(QStringLiteral("Folder..."), jsonTab);
    connect(btnFolder, &QPushButton::clicked, this, &Form::onChooseWorkspaceFolderClicked);

    QPushButton *btnImportJson = new QPushButton(QStringLiteral("Import..."), jsonTab);
    connect(btnImportJson, &QPushButton::clicked, this, &Form::onOpenJsonFileClicked);

    QPushButton *btnExportJson = new QPushButton(QStringLiteral("Export..."), jsonTab);
    connect(btnExportJson, &QPushButton::clicked, this, &Form::onSaveJsonFileClicked);

    QPushButton *btnSampleJson = new QPushButton(QStringLiteral("Sample"), jsonTab);
    connect(btnSampleJson, &QPushButton::clicked, this, &Form::onLoadSampleJsonClicked);

    jsonTopBar->addWidget(btnApplyJson);
    jsonTopBar->addWidget(btnFolder);
    jsonTopBar->addWidget(btnImportJson);
    jsonTopBar->addWidget(btnExportJson);
    jsonTopBar->addWidget(btnSampleJson);
    jsonTopBar->addStretch(1);

    m_jsonEditor = new QPlainTextEdit(jsonTab);
    m_jsonEditor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_jsonStatusLabel = new QLabel(QStringLiteral("Ready."), jsonTab);
    m_jsonStatusLabel->setStyleSheet(QStringLiteral("color: #10b981; font-size: 11px;"));

    jsonLayout->addLayout(jsonTopBar);
    jsonLayout->addWidget(m_jsonEditor);
    jsonLayout->addWidget(m_jsonStatusLabel);
    m_rightTabs->addTab(jsonTab, QStringLiteral("JSON File"));

    // Tab 2: PlantUML / Mermaid Script
    QWidget *scriptTab = new QWidget(m_rightTabs);
    QVBoxLayout *scriptLayout = new QVBoxLayout(scriptTab);
    scriptLayout->setContentsMargins(4, 4, 4, 4);

    QHBoxLayout *scriptTopBar = new QHBoxLayout();
    scriptTopBar->addWidget(new QLabel(QStringLiteral("Format:"), scriptTab));
    m_codeFormatCombo = new QComboBox(scriptTab);
    m_codeFormatCombo->addItem(QStringLiteral("Mermaid"));
    m_codeFormatCombo->addItem(QStringLiteral("PlantUML"));
    connect(m_codeFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Form::onExportFormatChanged);

    QPushButton *btnApplyScript = new QPushButton(QStringLiteral("Apply Script"), scriptTab);
    connect(btnApplyScript, &QPushButton::clicked, this, &Form::onApplyScriptClicked);

    QPushButton *btnCopyScript = new QPushButton(QStringLiteral("Copy"), scriptTab);
    connect(btnCopyScript, &QPushButton::clicked, this, &Form::onCopyScriptClicked);

    scriptTopBar->addWidget(m_codeFormatCombo);
    scriptTopBar->addStretch(1);
    scriptTopBar->addWidget(btnApplyScript);
    scriptTopBar->addWidget(btnCopyScript);

    m_codeEditor = new QPlainTextEdit(scriptTab);
    m_codeEditor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_codeStatusLabel = new QLabel(QStringLiteral("Mermaid / PlantUML preview."), scriptTab);
    m_codeStatusLabel->setStyleSheet(QStringLiteral("font-size: 11px;"));

    scriptLayout->addLayout(scriptTopBar);
    scriptLayout->addWidget(m_codeEditor);
    scriptLayout->addWidget(m_codeStatusLabel);
    m_rightTabs->addTab(scriptTab, QStringLiteral("PlantUML / Mermaid"));

    // Tab 3: Event Timeline
    QWidget *timelineTab = new QWidget(m_rightTabs);
    QVBoxLayout *timelineLayout = new QVBoxLayout(timelineTab);
    timelineLayout->setContentsMargins(4, 4, 4, 4);

    m_timelineTable = new QTableWidget(0, 5, timelineTab);
    m_timelineTable->setHorizontalHeaderLabels({
        QStringLiteral("DLT Index"),
        QStringLiteral("Time"),
        QStringLiteral("From"),
        QStringLiteral("To"),
        QStringLiteral("Transition Name")
    });
    m_timelineTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_timelineTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_timelineTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_timelineTable->setAlternatingRowColors(true);
    m_timelineTable->verticalHeader()->setVisible(false);
    m_timelineTable->horizontalHeader()->setStretchLastSection(true);
    m_timelineTable->setColumnWidth(0, 70);
    m_timelineTable->setColumnWidth(1, 85);
    m_timelineTable->setColumnWidth(2, 80);
    m_timelineTable->setColumnWidth(3, 80);
    connect(m_timelineTable, &QTableWidget::cellClicked,
            this, &Form::onTimelineRowClicked);
    connect(m_timelineTable, &QTableWidget::currentCellChanged,
            this, &Form::onTimelineCurrentCellChanged);

    timelineLayout->addWidget(m_timelineTable);
    m_rightTabs->addTab(timelineTab, QStringLiteral("Event Timeline"));

    filesPanelLayout->addWidget(m_rightTabs);
    m_filesPanel->setVisible(false); // Hidden by default; opened via Files -> Show Files

    splitter->addWidget(m_categoryTree);
    splitter->addWidget(m_graphicsView);
    splitter->addWidget(m_filesPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    splitter->setStretchFactor(2, 2);

    mainLayout->addWidget(splitter, 1);
}

void Form::setupMenus()
{
    QFrame *barFrame = new QFrame(this);
    barFrame->setObjectName(QStringLiteral("dltDiagramMenuBar"));
    barFrame->setStyleSheet(QStringLiteral(
        "QFrame#dltDiagramMenuBar {"
        "  background-color: #1e293b;"
        "  border-bottom: 1px solid #334155;"
        "}"
        "QToolButton {"
        "  background-color: transparent;"
        "  color: #f8fafc;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  padding: 4px 10px;"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "QToolButton:hover, QToolButton:pressed {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "}"
        "QToolButton::menu-indicator {"
        "  image: none;"
        "  width: 0px;"
        "}"));

    const QString popupMenuStyle = QStringLiteral(
        "QMenu {"
        "  background-color: #1e293b;"
        "  color: #f8fafc;"
        "  border: 1px solid #475569;"
        "  padding: 4px;"
        "  font-size: 12px;"
        "}"
        "QMenu::item {"
        "  background-color: transparent;"
        "  color: #f8fafc;"
        "  padding: 5px 24px 5px 12px;"
        "  border-radius: 3px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "}"
        "QMenu::item:disabled {"
        "  color: #64748b;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: #334155;"
        "  margin: 4px 6px;"
        "}");

    QHBoxLayout *menuLayout = new QHBoxLayout(barFrame);
    menuLayout->setContentsMargins(4, 2, 4, 2);
    menuLayout->setSpacing(4);

    auto createMenuButton = [barFrame, menuLayout, &popupMenuStyle](const QString &title) -> QMenu * {
        QToolButton *btn = new QToolButton(barFrame);
        btn->setText(title);
        btn->setPopupMode(QToolButton::InstantPopup);
        btn->setCursor(Qt::PointingHandCursor);
        QMenu *menu = new QMenu(btn);
        menu->setStyleSheet(popupMenuStyle);
        btn->setMenu(menu);
        menuLayout->addWidget(btn);
        return menu;
    };

    // 1. Files -> Show Files, Reload Diagram
    QMenu *filesMenu = createMenuButton(QStringLiteral("Files"));
    m_actShowFiles = filesMenu->addAction(QStringLiteral("Show Files"));
    m_actShowFiles->setCheckable(true);
    m_actShowFiles->setChecked(false);
    connect(m_actShowFiles, &QAction::triggered, this, &Form::onShowFilesDialogClicked);

    QAction *actReload = filesMenu->addAction(QStringLiteral("Reload Diagram"));
    connect(actReload, &QAction::triggered, this, &Form::onHotReloadAllClicked);

    // 2. Tabs -> Create, Edit, Delete, General Ordering, Favorite Toggle
    QMenu *tabsMenu = createMenuButton(QStringLiteral("Tabs"));
    connect(tabsMenu, &QMenu::aboutToShow, this, &Form::updateTabFavoriteActionState);

    QAction *actTabCreate = tabsMenu->addAction(QStringLiteral("Create"));
    connect(actTabCreate, &QAction::triggered, this, &Form::onAddTabClicked);

    QAction *actTabEdit = tabsMenu->addAction(QStringLiteral("Edit"));
    connect(actTabEdit, &QAction::triggered, this, &Form::onEditTabClicked);

    QAction *actTabDelete = tabsMenu->addAction(QStringLiteral("Delete"));
    connect(actTabDelete, &QAction::triggered, this, &Form::onDeleteTabClicked);

    tabsMenu->addSeparator();
    QAction *actTabOrdering = tabsMenu->addAction(QStringLiteral("General Ordering"));
    connect(actTabOrdering, &QAction::triggered, this, &Form::onGeneralTabOrderingClicked);

    m_actTabFavoriteToggle = tabsMenu->addAction(QStringLiteral("Favorite Toggle"));
    m_actTabFavoriteToggle->setCheckable(true);
    connect(m_actTabFavoriteToggle, &QAction::triggered, this, &Form::onToggleCurrentTabFavoriteClicked);

    // 3. State Diagram -> Create, Edit, Delete
    QMenu *stateMenu = createMenuButton(QStringLiteral("State Diagram"));
    connect(stateMenu->addAction(QStringLiteral("Create")), &QAction::triggered,
            this, &Form::onAddStateDiagramClicked);
    connect(stateMenu->addAction(QStringLiteral("Edit")), &QAction::triggered,
            this, &Form::onEditStateDiagramClicked);
    connect(stateMenu->addAction(QStringLiteral("Delete")), &QAction::triggered,
            this, &Form::onDeleteStateDiagramClicked);

    // 4. Sequence Diagram -> Create, Edit, Delete
    QMenu *seqMenu = createMenuButton(QStringLiteral("Sequence Diagram"));
    connect(seqMenu->addAction(QStringLiteral("Create")), &QAction::triggered,
            this, &Form::onAddSequenceDiagramClicked);
    connect(seqMenu->addAction(QStringLiteral("Edit")), &QAction::triggered,
            this, &Form::onEditSequenceDiagramClicked);
    connect(seqMenu->addAction(QStringLiteral("Delete")), &QAction::triggered,
            this, &Form::onDeleteSequenceDiagramClicked);

    // 5. Flow Diagram -> Create, Edit, Delete
    QMenu *flowMenu = createMenuButton(QStringLiteral("Flow Diagram"));
    connect(flowMenu->addAction(QStringLiteral("Create")), &QAction::triggered,
            this, &Form::onAddFlowDiagramClicked);
    connect(flowMenu->addAction(QStringLiteral("Edit")), &QAction::triggered,
            this, &Form::onEditFlowDiagramClicked);
    connect(flowMenu->addAction(QStringLiteral("Delete")), &QAction::triggered,
            this, &Form::onDeleteFlowDiagramClicked);

    // 6. Favorite -> List
    QMenu *favMenu = createMenuButton(QStringLiteral("Favorite"));
    connect(favMenu->addAction(QStringLiteral("List")), &QAction::triggered,
            this, &Form::onShowFavoriteListDialogClicked);

    menuLayout->addStretch(1);
    m_menuBarWidget = barFrame;
}

int Form::currentDiagramIndex() const
{
    return m_currentDiagramIdx;
}

bool Form::isFavoritesTabSelected() const
{
    if (!m_tabBar || m_tabBar->currentIndex() < 0) {
        return false;
    }
    return m_tabBar->tabData(m_tabBar->currentIndex()).toString() == kVirtualFavoritesTabId;
}

QString Form::currentTabName() const
{
    if (!m_tabBar || m_tabBar->currentIndex() < 0) {
        return QStringLiteral("Default");
    }
    const QString data = m_tabBar->tabData(m_tabBar->currentIndex()).toString();
    if (data == kVirtualFavoritesTabId) {
        // If on the ★ Favorite virtual tab, return the current diagram's tabName or first tabName
        if (m_engine && m_currentDiagramIdx >= 0 && m_currentDiagramIdx < m_engine->diagramCount()) {
            return m_engine->diagramAt(m_currentDiagramIdx).tabName;
        }
        const QStringList allTabs = m_engine ? m_engine->tabNames() : QStringList();
        return allTabs.isEmpty() ? QStringLiteral("Default") : allTabs.first();
    }
    return data.isEmpty() ? m_tabBar->tabText(m_tabBar->currentIndex()) : data;
}

bool Form::isTimeTravelMode() const
{
    return m_syncModeCombo && (m_syncModeCombo->currentIndex() == 0);
}

void Form::onConfigUpdated(const QString &rawJsonText)
{
    Q_UNUSED(rawJsonText);
    m_updatingUi = true;
    refreshTabBar();
    refreshCategoryTree();
    refreshTabJsonEditor();
    m_updatingUi = false;

    rebuildEventTimelineTable();
    refreshDiagramAndCodeView();
}

void Form::refreshTabBar()
{
    if (!m_engine || !m_tabBar) return;

    const QString prevTabData = (m_tabBar->currentIndex() >= 0)
                                    ? m_tabBar->tabData(m_tabBar->currentIndex()).toString()
                                    : QString();

    // m_engine->tabNames() returns Favorite Tabs first, then General Tabs!
    const QStringList orderedTabs = m_engine->tabNames();

    m_tabBar->blockSignals(true);
    while (m_tabBar->count() > 0) {
        m_tabBar->removeTab(0);
    }

    int targetTabIdx = -1;

    // 1. Dedicated '★ Favorite' Tab FIRST (leftmost, index 0)
    const int favTabIdx = m_tabBar->addTab(QStringLiteral("★ Favorite"));
    m_tabBar->setTabData(favTabIdx, kVirtualFavoritesTabId);
    m_tabBar->setTabTextColor(favTabIdx, QColor(245, 158, 11));
    m_tabBar->setTabToolTip(favTabIdx, QStringLiteral("Favorite Diagrams List (Add / Remove Favorite Diagrams)"));
    if (prevTabData == kVirtualFavoritesTabId) {
        targetTabIdx = favTabIdx;
    }

    // 2. Add user Tabs: Favorite Tabs are shown next (with ★ prefix), followed by General Tabs
    for (const QString &tName : orderedTabs) {
        const bool isFavTab = m_engine->isTabFavorite(tName);
        const QString displayLabel = isFavTab ? QStringLiteral("★ %1").arg(tName) : tName;
        const int idx = m_tabBar->addTab(displayLabel);
        m_tabBar->setTabData(idx, tName);

        const QColor tColor = m_engine->tabColor(tName);
        if (tColor.isValid()) {
            m_tabBar->setTabIcon(idx, createColorSwatchIcon(tColor));
            m_tabBar->setTabTextColor(idx, tColor);
        } else if (isFavTab) {
            m_tabBar->setTabTextColor(idx, QColor(245, 158, 11));
        }

        m_tabBar->setTabToolTip(
            idx,
            QStringLiteral("Tab: %1 (%2.json)%3")
                .arg(tName,
                     DiagramStateEngine::sanitizeTabFileName(tName),
                     isFavTab ? QStringLiteral(" [★ Favorite Tab]") : QString()));

        if (tName == prevTabData) {
            targetTabIdx = idx;
        }
    }

    if (targetTabIdx < 0 && m_tabBar->count() > 0) {
        // On initial start, select ★ Favorite (index 0) if user has favorite diagrams, otherwise select first user tab (index 1)
        if (!m_engine->favoriteDiagramsSorted().isEmpty() || m_tabBar->count() == 1) {
            targetTabIdx = 0;
        } else {
            targetTabIdx = 1;
        }
    }
    if (targetTabIdx >= 0) {
        m_tabBar->setCurrentIndex(targetTabIdx);
    }
    m_tabBar->blockSignals(false);
    updateTabFavoriteActionState();
}

void Form::updateTabFavoriteActionState()
{
    if (!m_actTabFavoriteToggle || !m_engine) return;
    if (isFavoritesTabSelected()) {
        m_actTabFavoriteToggle->setEnabled(false);
        m_actTabFavoriteToggle->setChecked(false);
        return;
    }
    m_actTabFavoriteToggle->setEnabled(true);
    const QString tabName = currentTabName();
    m_actTabFavoriteToggle->setChecked(m_engine->isTabFavorite(tabName));
}

void Form::refreshCategoryTree()
{
    if (!m_engine || !m_categoryTree) return;

    m_categoryTree->blockSignals(true);
    m_categoryTree->clear();

    QFont boldFont = m_categoryTree->font();
    boldFont.setBold(true);

    int firstAvailableIdx = -1;
    bool currentIdxStillVisible = false;
    QTreeWidgetItem *selectedTreeItem = nullptr;

    auto addDiagramRow = [this, &firstAvailableIdx, &currentIdxStillVisible, &selectedTreeItem](
                             QTreeWidgetItem *parentItem,
                             const DiagramDefinition &d,
                             bool showTabSuffix,
                             bool inFavGroup) {
        const int flatIdx = m_engine->findDiagramIndex(d.tabName, d.id);
        if (firstAvailableIdx < 0) {
            firstAvailableIdx = flatIdx;
        }
        const bool isFav = m_engine->isFavorite(d.favoriteKey());
        const QString titleText = showTabSuffix
                                      ? QStringLiteral("%1 (%2)").arg(d.title, d.tabName)
                                      : d.title;
        const QString starText = isFav ? QStringLiteral("★") : QStringLiteral("☆");

        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem, {titleText, starText});
        item->setData(0, kRoleDiagramIndex, flatIdx);
        item->setData(0, kRoleFavoriteKey, d.favoriteKey());
        item->setData(0, Qt::UserRole + 2, inFavGroup);
        item->setTextAlignment(1, Qt::AlignCenter);
        item->setForeground(1, isFav ? QColor(245, 158, 11) : QColor(148, 163, 184));
        item->setToolTip(0, QStringLiteral("%1 (Tab: %2)\nDouble-click to Edit").arg(d.title, d.tabName));
        item->setToolTip(1, isFav
                                ? QStringLiteral("Click ★ to remove '%1' from Favorites").arg(d.title)
                                : QStringLiteral("Click ☆ to add '%1' to Favorites").arg(d.title));

        if (flatIdx == m_currentDiagramIdx) {
            currentIdxStillVisible = true;
            if (!selectedTreeItem || (inFavGroup == m_preferFavoriteGroupInTree)) {
                selectedTreeItem = item;
            }
        }
    };

    struct CategoryInfo {
        DiagramType type;
        QString label;
    };
    const QList<CategoryInfo> categories = {
        {DiagramType::StateDiagram, QStringLiteral("State Diagram")},
        {DiagramType::SequenceDiagram, QStringLiteral("Sequence Diagram")},
        {DiagramType::FlowDiagram, QStringLiteral("Flow Diagram")}
    };

    if (isFavoritesTabSelected()) {
        // Dedicated '★ Favorite' Tab selected: Show Favorite Diagrams + All Diagrams to easily Add/Remove Favorites!
        QTreeWidgetItem *favRoot = new QTreeWidgetItem(m_categoryTree, {QStringLiteral("★ Favorite Diagrams"), QString()});
        favRoot->setFlags(Qt::ItemIsEnabled);
        favRoot->setFont(0, boldFont);
        favRoot->setForeground(0, QColor(245, 158, 11));

        const QList<DiagramDefinition> favList = m_engine->favoriteDiagramsSorted();
        for (const DiagramDefinition &d : favList) {
            addDiagramRow(favRoot, d, true, true);
        }
        favRoot->setExpanded(true);

        // Also show All Diagrams below so the user can click ☆/★ directly in the Favorite tab list to add/remove!
        QTreeWidgetItem *allRoot = new QTreeWidgetItem(
            m_categoryTree, {QStringLiteral("All Diagrams (Click ☆/★ to Add/Remove)"), QString()});
        allRoot->setFlags(Qt::ItemIsEnabled);
        allRoot->setFont(0, boldFont);
        allRoot->setForeground(0, QColor(59, 130, 246));

        const QList<DiagramDefinition> allDiags = m_engine->diagrams();
        for (const DiagramDefinition &d : allDiags) {
            addDiagramRow(allRoot, d, true, false);
        }
        allRoot->setExpanded(true);
    } else {
        // Regular or Favorite Tab selected:
        const QString activeTab = currentTabName();

        // 1. Separate Favorite Diagrams section at top
        const QList<DiagramDefinition> favList = m_engine->favoriteDiagramsSorted();
        if (!favList.isEmpty()) {
            QTreeWidgetItem *favRoot = new QTreeWidgetItem(
                m_categoryTree,
                {QStringLiteral("★ Favorite (%1)").arg(favList.size()), QString()});
            favRoot->setFlags(Qt::ItemIsEnabled);
            favRoot->setFont(0, boldFont);
            favRoot->setForeground(0, QColor(245, 158, 11));
            for (const DiagramDefinition &d : favList) {
                addDiagramRow(favRoot, d, true, true);
            }
            favRoot->setExpanded(true);
        }

        // 2. Current Tab's State / Sequence / Flow Diagrams (sorted A-Z)
        for (const CategoryInfo &cat : categories) {
            const QList<DiagramDefinition> list = m_engine->diagramsForTabAndType(activeTab, cat.type);
            QTreeWidgetItem *catRoot = new QTreeWidgetItem(
                m_categoryTree,
                {QStringLiteral("%1 (%2)").arg(cat.label).arg(list.size()), QString()});
            catRoot->setFlags(Qt::ItemIsEnabled);
            catRoot->setFont(0, boldFont);
            catRoot->setForeground(0, QColor(59, 130, 246));

            for (const DiagramDefinition &d : list) {
                addDiagramRow(catRoot, d, false, false);
            }
            catRoot->setExpanded(true);
        }
    }

    if (!currentIdxStillVisible) {
        m_currentDiagramIdx = firstAvailableIdx;
    }

    if (!selectedTreeItem && m_currentDiagramIdx >= 0) {
        // Find the tree item corresponding to m_currentDiagramIdx (firstAvailableIdx)
        for (int i = 0; i < m_categoryTree->topLevelItemCount() && !selectedTreeItem; ++i) {
            QTreeWidgetItem *top = m_categoryTree->topLevelItem(i);
            for (int c = 0; c < top->childCount(); ++c) {
                QTreeWidgetItem *child = top->child(c);
                if (child->data(0, kRoleDiagramIndex).toInt() == m_currentDiagramIdx) {
                    selectedTreeItem = child;
                    break;
                }
            }
        }
    }

    if (selectedTreeItem) {
        m_categoryTree->setCurrentItem(selectedTreeItem);
    }

    m_categoryTree->blockSignals(false);
}

void Form::refreshTabJsonEditor()
{
    if (!m_engine || !m_jsonEditor) return;
    const QString activeTab = currentTabName();
    const QList<DiagramDefinition> tabDiags = m_engine->diagramsForTab(activeTab);
    const QColor tCol = m_engine->tabColor(activeTab);
    m_jsonEditor->setPlainText(
        DiagramParser::toJsonConfig(tabDiags, activeTab, tCol.isValid() ? tCol.name(QColor::HexRgb) : QString()));
    if (m_rightTabs) {
        m_rightTabs->setTabText(0, QStringLiteral("%1.json").arg(activeTab));
    }
}

// --- Files Menu Actions ---

void Form::onShowFilesDialogClicked()
{
    if (!m_filesPanel) return;
    const bool show = !m_filesPanel->isVisible();
    m_filesPanel->setVisible(show);
    if (m_actShowFiles) {
        m_actShowFiles->blockSignals(true);
        m_actShowFiles->setChecked(show);
        m_actShowFiles->blockSignals(false);
    }
    if (show) {
        refreshTabJsonEditor();
    }
}

void Form::onHotReloadAllClicked()
{
    if (!m_engine) return;
    QString err;
    m_engine->loadTabsFromDirectory(m_engine->workspaceDirectory(), err);
    onConfigUpdated();
    emit rescanFileRequested();
    if (m_jsonStatusLabel) {
        m_jsonStatusLabel->setText(QStringLiteral("Reloaded all diagram JSON files and rescanned DLT logs."));
    }
}

// --- Tabs Menu & Tab Bar Actions ---

void Form::onTabBarCurrentChanged(int index)
{
    Q_UNUSED(index);
    if (m_updatingUi) return;

    updateTabFavoriteActionState();
    refreshCategoryTree();
    refreshTabJsonEditor();
    rebuildEventTimelineTable();
    refreshDiagramAndCodeView();
}

void Form::onTabBarMoved(int from, int to)
{
    Q_UNUSED(from);
    Q_UNUSED(to);
    if (!m_engine || !m_tabBar) return;

    QStringList newOrder;
    for (int i = 0; i < m_tabBar->count(); ++i) {
        const QString data = m_tabBar->tabData(i).toString();
        if (!data.isEmpty() && data != kVirtualFavoritesTabId) {
            newOrder.append(data);
        }
    }
    m_engine->setTabOrder(newOrder);
    refreshTabBar();
    refreshCategoryTree();
}

void Form::onTabBarContextMenuRequested(const QPoint &pos)
{
    if (!m_tabBar || !m_engine) return;
    const int tabIdx = m_tabBar->tabAt(pos);
    if (tabIdx >= 0) {
        m_tabBar->setCurrentIndex(tabIdx);
    }
    if (isFavoritesTabSelected()) {
        onShowFavoriteListDialogClicked();
        return;
    }

    const QString tabName = currentTabName();
    const bool isFav = m_engine->isTabFavorite(tabName);

    QMenu menu(this);
    QAction *actFav = menu.addAction(
        isFav ? QStringLiteral("★ Remove Tab from Favorites") : QStringLiteral("☆ Add Tab to Favorites"));
    menu.addSeparator();
    QAction *actEdit = menu.addAction(QStringLiteral("Edit Tab (Name & Color)..."));
    QAction *actOrder = menu.addAction(QStringLiteral("General Ordering..."));
    QAction *actDelete = menu.addAction(QStringLiteral("Delete Tab..."));

    QAction *chosen = menu.exec(m_tabBar->mapToGlobal(pos));
    if (chosen == actFav) {
        onToggleCurrentTabFavoriteClicked();
    } else if (chosen == actEdit) {
        onEditTabClicked();
    } else if (chosen == actOrder) {
        onGeneralTabOrderingClicked();
    } else if (chosen == actDelete) {
        onDeleteTabClicked();
    }
}

void Form::onAddTabClicked()
{
    if (!m_engine) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Create Tab"));
    QFormLayout *form = new QFormLayout(&dlg);

    QLineEdit *nameEdit = new QLineEdit(QStringLiteral("NewTab"), &dlg);
    QColor pickedColor;
    QPushButton *colorBtn = new QPushButton(QStringLiteral("Select Tab Color..."), &dlg);
    connect(colorBtn, &QPushButton::clicked, &dlg, [&dlg, &pickedColor, colorBtn]() {
        const QColor c = QColorDialog::getColor(
            pickedColor.isValid() ? pickedColor : QColor(37, 99, 235), &dlg, QStringLiteral("Select Tab Color"));
        if (c.isValid()) {
            pickedColor = c;
            colorBtn->setText(c.name(QColor::HexRgb));
            colorBtn->setStyleSheet(QStringLiteral("background-color: %1; color: white; font-weight: bold;")
                                        .arg(c.name(QColor::HexRgb)));
        }
    });

    form->addRow(QStringLiteral("Tab Name:"), nameEdit);
    form->addRow(QStringLiteral("Tab Color:"), colorBtn);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString tabName = nameEdit->text().trimmed();
    if (tabName.isEmpty()) {
        return;
    }

    QString err;
    if (!m_engine->addTab(tabName, err)) {
        QMessageBox::warning(this, QStringLiteral("Create Tab"), err);
        return;
    }
    if (pickedColor.isValid()) {
        m_engine->setTabColor(tabName, pickedColor);
    }

    refreshTabBar();
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == tabName) {
            m_tabBar->setCurrentIndex(i);
            break;
        }
    }
    refreshCategoryTree();
    refreshTabJsonEditor();
    rebuildEventTimelineTable();
    refreshDiagramAndCodeView();
}

void Form::onEditTabClicked()
{
    if (!m_engine) return;
    const QString oldName = currentTabName();
    QColor currentColor = m_engine->tabColor(oldName);

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Edit Tab '%1'").arg(oldName));
    QFormLayout *form = new QFormLayout(&dlg);

    QLineEdit *nameEdit = new QLineEdit(oldName, &dlg);
    QHBoxLayout *colorRow = new QHBoxLayout();
    QPushButton *colorBtn = new QPushButton(
        currentColor.isValid() ? currentColor.name(QColor::HexRgb) : QStringLiteral("Select Tab Color..."), &dlg);
    if (currentColor.isValid()) {
        colorBtn->setStyleSheet(QStringLiteral("background-color: %1; color: white; font-weight: bold;")
                                    .arg(currentColor.name(QColor::HexRgb)));
    }
    QPushButton *resetColorBtn = new QPushButton(QStringLiteral("Reset"), &dlg);
    connect(colorBtn, &QPushButton::clicked, &dlg, [&dlg, &currentColor, colorBtn]() {
        const QColor c = QColorDialog::getColor(
            currentColor.isValid() ? currentColor : QColor(37, 99, 235), &dlg, QStringLiteral("Select Tab Color"));
        if (c.isValid()) {
            currentColor = c;
            colorBtn->setText(c.name(QColor::HexRgb));
            colorBtn->setStyleSheet(QStringLiteral("background-color: %1; color: white; font-weight: bold;")
                                        .arg(c.name(QColor::HexRgb)));
        }
    });
    connect(resetColorBtn, &QPushButton::clicked, &dlg, [&currentColor, colorBtn]() {
        currentColor = QColor();
        colorBtn->setText(QStringLiteral("Default Color"));
        colorBtn->setStyleSheet(QString());
    });
    colorRow->addWidget(colorBtn, 1);
    colorRow->addWidget(resetColorBtn);

    form->addRow(QStringLiteral("Tab Name:"), nameEdit);
    form->addRow(QStringLiteral("Tab Color:"), colorRow);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString newName = nameEdit->text().trimmed();
    if (newName.isEmpty()) {
        return;
    }

    QString targetTab = oldName;
    if (newName != oldName) {
        QString err;
        if (!m_engine->renameTab(oldName, newName, err)) {
            QMessageBox::warning(this, QStringLiteral("Edit Tab"), err);
            return;
        }
        targetTab = newName;
    }
    m_engine->setTabColor(targetTab, currentColor);
    onConfigUpdated();
}

void Form::onDeleteTabClicked()
{
    if (!m_engine) return;
    if (isFavoritesTabSelected()) {
        return;
    }
    const QString tab = currentTabName();
    const auto reply = QMessageBox::question(
        this,
        QStringLiteral("Delete Tab"),
        QStringLiteral("Delete Tab '%1' and remove '%1.json'?").arg(tab),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    QString err;
    m_engine->removeTab(tab, true, err);
    onConfigUpdated();
    emit rescanFileRequested();
}

void Form::onGeneralTabOrderingClicked()
{
    if (!m_engine) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Tabs - General Ordering"));
    dlg.resize(360, 320);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        QStringLiteral("Drag or use Up/Down buttons to change Tab order.\n(Note: ★ Favorite Tabs are always shown first.)"),
        &dlg));

    QListWidget *listWidget = new QListWidget(&dlg);
    listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    for (const QString &t : m_engine->generalTabOrder()) {
        QListWidgetItem *item = new QListWidgetItem(
            m_engine->isTabFavorite(t) ? QStringLiteral("★ %1").arg(t) : t,
            listWidget);
        item->setData(Qt::UserRole, t);
    }
    lay->addWidget(listWidget, 1);

    QHBoxLayout *moveBtns = new QHBoxLayout();
    QPushButton *btnUp = new QPushButton(QStringLiteral("▲ Move Up"), &dlg);
    QPushButton *btnDown = new QPushButton(QStringLiteral("▼ Move Down"), &dlg);
    connect(btnUp, &QPushButton::clicked, listWidget, [listWidget]() {
        const int r = listWidget->currentRow();
        if (r > 0) {
            QListWidgetItem *it = listWidget->takeItem(r);
            listWidget->insertItem(r - 1, it);
            listWidget->setCurrentRow(r - 1);
        }
    });
    connect(btnDown, &QPushButton::clicked, listWidget, [listWidget]() {
        const int r = listWidget->currentRow();
        if (r >= 0 && r + 1 < listWidget->count()) {
            QListWidgetItem *it = listWidget->takeItem(r);
            listWidget->insertItem(r + 1, it);
            listWidget->setCurrentRow(r + 1);
        }
    });
    moveBtns->addWidget(btnUp);
    moveBtns->addWidget(btnDown);
    lay->addLayout(moveBtns);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        QStringList newOrder;
        for (int i = 0; i < listWidget->count(); ++i) {
            newOrder.append(listWidget->item(i)->data(Qt::UserRole).toString());
        }
        m_engine->setTabOrder(newOrder);
        refreshTabBar();
        refreshCategoryTree();
    }
}

void Form::onToggleCurrentTabFavoriteClicked()
{
    if (!m_engine || isFavoritesTabSelected()) return;
    const QString tabName = currentTabName();
    m_engine->toggleTabFavorite(tabName);
    refreshTabBar();
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == tabName) {
            m_tabBar->setCurrentIndex(i);
            break;
        }
    }
    updateTabFavoriteActionState();
}

// --- State / Sequence / Flow Diagram Menu Actions ---

void Form::openBuilderDialogForType(DiagramType type, const DiagramDefinition *existingDef)
{
    if (!m_engine) return;

    const QString initTab = existingDef ? existingDef->tabName : currentTabName();
    DiagramBuilderDialog dlg(
        m_engine->tabNames(),
        initTab,
        m_engine->tabColor(initTab),
        type,
        this);
    if (existingDef) {
        dlg.loadDefinition(*existingDef);
    }

    if (dlg.exec() == QDialog::Accepted) {
        const DiagramDefinition built = dlg.builtDefinition();
        QString err;
        m_currentDiagramIdx = m_engine->addOrUpdateDiagram(built, true, err);
        m_engine->setTabColor(built.tabName, dlg.selectedTabColor());

        refreshTabBar();
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabData(i).toString() == built.tabName) {
                m_tabBar->setCurrentIndex(i);
                break;
            }
        }
        refreshCategoryTree();
        refreshTabJsonEditor();
        emit rescanFileRequested();
    }
}

int Form::resolveTargetDiagramForType(DiagramType type, const QString &actionTitle)
{
    if (!m_engine || m_engine->diagramCount() == 0) {
        return -1;
    }

    // If the currently selected diagram already matches 'type', use it directly
    if (m_currentDiagramIdx >= 0 && m_currentDiagramIdx < m_engine->diagramCount()) {
        const DiagramDefinition cur = m_engine->diagramAt(m_currentDiagramIdx);
        if (cur.type == type) {
            return m_currentDiagramIdx;
        }
    }

    // Otherwise find diagrams of 'type' in currentTabName()
    const QString activeTab = currentTabName();
    const QList<DiagramDefinition> list = m_engine->diagramsForTabAndType(activeTab, type);
    if (list.isEmpty()) {
        QMessageBox::information(
            this,
            actionTitle,
            QStringLiteral("No %1 found in Tab '%2'.")
                .arg(DiagramDefinition::typeToString(type), activeTab));
        return -1;
    }
    if (list.size() == 1) {
        return m_engine->findDiagramIndex(list.first().tabName, list.first().id);
    }

    QStringList titles;
    for (const DiagramDefinition &d : list) {
        titles.append(d.title);
    }
    bool ok = false;
    const QString picked = QInputDialog::getItem(
        this, actionTitle, QStringLiteral("Select Diagram:"), titles, 0, false, &ok);
    if (!ok || picked.isEmpty()) {
        return -1;
    }
    const int idxInList = titles.indexOf(picked);
    if (idxInList >= 0) {
        return m_engine->findDiagramIndex(list[idxInList].tabName, list[idxInList].id);
    }
    return -1;
}

void Form::editDiagramByType(DiagramType type)
{
    const int targetIdx = resolveTargetDiagramForType(type, QStringLiteral("Edit Diagram"));
    if (targetIdx < 0) return;
    m_currentDiagramIdx = targetIdx;
    const DiagramDefinition def = m_engine->diagramAt(targetIdx);
    openBuilderDialogForType(def.type, &def);
}

void Form::deleteDiagramByType(DiagramType type)
{
    const int targetIdx = resolveTargetDiagramForType(type, QStringLiteral("Delete Diagram"));
    if (targetIdx < 0) return;

    const DiagramDefinition def = m_engine->diagramAt(targetIdx);
    const auto ans = QMessageBox::question(
        this,
        QStringLiteral("Delete Diagram"),
        QStringLiteral("Delete '%1' from Tab '%2'?").arg(def.title, def.tabName),
        QMessageBox::Yes | QMessageBox::No);
    if (ans != QMessageBox::Yes) {
        return;
    }

    QString err;
    m_engine->deleteDiagram(def.tabName, def.id, true, err);
    onConfigUpdated();
    emit rescanFileRequested();
}

void Form::onAddStateDiagramClicked()      { openBuilderDialogForType(DiagramType::StateDiagram, nullptr); }
void Form::onEditStateDiagramClicked()     { editDiagramByType(DiagramType::StateDiagram); }
void Form::onDeleteStateDiagramClicked()   { deleteDiagramByType(DiagramType::StateDiagram); }

void Form::onAddSequenceDiagramClicked()    { openBuilderDialogForType(DiagramType::SequenceDiagram, nullptr); }
void Form::onEditSequenceDiagramClicked()   { editDiagramByType(DiagramType::SequenceDiagram); }
void Form::onDeleteSequenceDiagramClicked() { deleteDiagramByType(DiagramType::SequenceDiagram); }

void Form::onAddFlowDiagramClicked()       { openBuilderDialogForType(DiagramType::FlowDiagram, nullptr); }
void Form::onEditFlowDiagramClicked()      { editDiagramByType(DiagramType::FlowDiagram); }
void Form::onDeleteFlowDiagramClicked()    { deleteDiagramByType(DiagramType::FlowDiagram); }

// --- Favorite -> List Dialog (Manage both Tab Favorites & Diagram Favorites) ---

void Form::onShowFavoriteListDialogClicked()
{
    if (!m_engine) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Favorite List (Tabs & Diagrams)"));
    dlg.resize(480, 440);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        QStringLiteral("<b>1. Tab Favorites</b> (Checked Tabs are shown first on the Tab Bar):"), &dlg));
    QListWidget *tabList = new QListWidget(&dlg);
    for (const QString &tName : m_engine->generalTabOrder()) {
        QListWidgetItem *item = new QListWidgetItem(tName, tabList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_engine->isTabFavorite(tName) ? Qt::Checked : Qt::Unchecked);
    }
    lay->addWidget(tabList, 1);

    lay->addWidget(new QLabel(
        QStringLiteral("<b>2. Diagram Favorites</b> (Check/Uncheck to Add/Remove from Favorite List):"), &dlg));
    QListWidget *diagList = new QListWidget(&dlg);
    for (const DiagramDefinition &d : m_engine->diagrams()) {
        QListWidgetItem *item = new QListWidgetItem(
            QStringLiteral("[%1] %2 (%3)")
                .arg(DiagramDefinition::typeToString(d.type).toUpper(), d.title, d.tabName),
            diagList);
        item->setData(Qt::UserRole, d.favoriteKey());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_engine->isFavorite(d.favoriteKey()) ? Qt::Checked : Qt::Unchecked);
    }
    lay->addWidget(diagList, 2);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        for (int i = 0; i < tabList->count(); ++i) {
            QListWidgetItem *it = tabList->item(i);
            m_engine->setTabFavorite(it->text(), it->checkState() == Qt::Checked);
        }
        for (int i = 0; i < diagList->count(); ++i) {
            QListWidgetItem *it = diagList->item(i);
            m_engine->setFavorite(it->data(Qt::UserRole).toString(), it->checkState() == Qt::Checked);
        }
        refreshTabBar();
        refreshCategoryTree();
    }
}

// --- Diagram List Keyboard Navigation / Click / Inline Favorite Add & Remove / Context Menu ---

void Form::onCategoryTreeCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (!current || !m_engine || m_updatingUi) {
        return;
    }

    const QVariant idxVar = current->data(0, kRoleDiagramIndex);
    if (!idxVar.isValid()) {
        return;
    }

    m_preferFavoriteGroupInTree = current->data(0, Qt::UserRole + 2).toBool();

    const int flatIdx = idxVar.toInt();
    if (flatIdx < 0 || flatIdx >= m_engine->diagramCount()) {
        return;
    }

    if (flatIdx != m_currentDiagramIdx) {
        m_currentDiagramIdx = flatIdx;
        rebuildEventTimelineTable();
        refreshDiagramAndCodeView();
    }
}

void Form::onCategoryTreeItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item || !m_engine) return;
    const QVariant idxVar = item->data(0, kRoleDiagramIndex);
    if (!idxVar.isValid()) {
        return;
    }

    m_preferFavoriteGroupInTree = item->data(0, Qt::UserRole + 2).toBool();

    const int flatIdx = idxVar.toInt();
    const QString favKey = item->data(0, kRoleFavoriteKey).toString();

    // Column 1 is the inline '★ / ☆' Favorite Add/Remove button!
    if (column == 1 && !favKey.isEmpty()) {
        m_engine->toggleFavorite(favKey);
        refreshCategoryTree();
        return;
    }

    if (flatIdx < 0 || flatIdx >= m_engine->diagramCount()) {
        return;
    }

    m_currentDiagramIdx = flatIdx;
    rebuildEventTimelineTable();
    refreshDiagramAndCodeView();
}

void Form::onCategoryTreeItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!item || !m_engine || column == 1) return;
    const QVariant idxVar = item->data(0, kRoleDiagramIndex);
    if (!idxVar.isValid()) {
        return;
    }
    const int flatIdx = idxVar.toInt();
    if (flatIdx >= 0 && flatIdx < m_engine->diagramCount()) {
        m_currentDiagramIdx = flatIdx;
        const DiagramDefinition def = m_engine->diagramAt(flatIdx);
        openBuilderDialogForType(def.type, &def);
    }
}

void Form::onCategoryTreeContextMenuRequested(const QPoint &pos)
{
    if (!m_categoryTree || !m_engine) return;
    QTreeWidgetItem *item = m_categoryTree->itemAt(pos);
    if (!item) return;

    const QVariant idxVar = item->data(0, kRoleDiagramIndex);
    if (!idxVar.isValid()) return;

    m_preferFavoriteGroupInTree = item->data(0, Qt::UserRole + 2).toBool();

    const int flatIdx = idxVar.toInt();
    if (flatIdx < 0 || flatIdx >= m_engine->diagramCount()) return;

    m_currentDiagramIdx = flatIdx;
    const DiagramDefinition def = m_engine->diagramAt(flatIdx);
    const bool isFav = m_engine->isFavorite(def.favoriteKey());

    QMenu menu(this);
    QAction *actFav = menu.addAction(
        isFav ? QStringLiteral("★ Remove from Favorite") : QStringLiteral("☆ Add to Favorite"));
    menu.addSeparator();
    QAction *actEdit = menu.addAction(QStringLiteral("Edit Diagram..."));
    QAction *actDel = menu.addAction(QStringLiteral("Delete Diagram..."));

    QAction *chosen = menu.exec(m_categoryTree->viewport()->mapToGlobal(pos));
    if (chosen == actFav) {
        m_engine->toggleFavorite(def.favoriteKey());
        refreshCategoryTree();
    } else if (chosen == actEdit) {
        openBuilderDialogForType(def.type, &def);
    } else if (chosen == actDel) {
        deleteDiagramByType(def.type);
    }
}

// --- Sync, Timeline, Script & JSON Slots ---

void Form::onFileReloaded()
{
    m_selectedDltIndex = -1;
    m_latestDltIndex = -1;
    rebuildEventTimelineTable();
    refreshDiagramAndCodeView();
}

void Form::onLiveEventsUpdated(int latestDltIndex)
{
    m_latestDltIndex = latestDltIndex;
    rebuildEventTimelineTable();
    if (!isTimeTravelMode() || m_selectedDltIndex < 0) {
        refreshDiagramAndCodeView();
    }
}

void Form::onSelectedLogChanged(int selectedDltIndex)
{
    m_selectedDltIndex = selectedDltIndex;
    if (isTimeTravelMode()) {
        refreshDiagramAndCodeView();
    }
}

void Form::onSyncModeChanged(int index)
{
    Q_UNUSED(index);
    refreshDiagramAndCodeView();
}

void Form::onExportFormatChanged(int index)
{
    Q_UNUSED(index);
    refreshDiagramAndCodeView();
}

void Form::rebuildEventTimelineTable()
{
    if (!m_engine || !m_timelineTable) {
        return;
    }

    const int diagIdx = currentDiagramIndex();
    const QVector<DiagramEvent> events = m_engine->eventsForDiagram(diagIdx);

    m_timelineTable->blockSignals(true);
    m_timelineTable->setUpdatesEnabled(false);
    m_timelineTable->setRowCount(events.size());

    for (int i = 0; i < events.size(); ++i) {
        const DiagramEvent &ev = events[i];

        QTableWidgetItem *idxItem = new QTableWidgetItem(QString::number(ev.dltIndex));
        idxItem->setData(Qt::UserRole, ev.dltIndex);
        QTableWidgetItem *timeItem = new QTableWidgetItem(ev.timeString);
        QTableWidgetItem *fromItem = new QTableWidgetItem(ev.from);
        QTableWidgetItem *toItem = new QTableWidgetItem(ev.to);
        QTableWidgetItem *lblItem = new QTableWidgetItem(ev.label);
        lblItem->setToolTip(ev.payload);

        m_timelineTable->setItem(i, 0, idxItem);
        m_timelineTable->setItem(i, 1, timeItem);
        m_timelineTable->setItem(i, 2, fromItem);
        m_timelineTable->setItem(i, 3, toItem);
        m_timelineTable->setItem(i, 4, lblItem);
    }

    m_timelineTable->setUpdatesEnabled(true);
    m_timelineTable->blockSignals(false);
    if (m_eventCountLabel) {
        m_eventCountLabel->setText(QStringLiteral("Events: %1").arg(events.size()));
    }
}

void Form::refreshDiagramAndCodeView()
{
    if (!m_engine) {
        return;
    }

    const int diagIdx = currentDiagramIndex();
    if (diagIdx < 0 || diagIdx >= m_engine->diagramCount()) {
        if (m_currentStateBadge) {
            m_currentStateBadge->setText(QStringLiteral("State: -"));
        }
        if (m_graphicsView) {
            m_graphicsView->clearCanvasWithMessage(
                QStringLiteral("No diagram selected in this tab.\nUse 'State / Sequence / Flow Diagram -> Create' menu to add a diagram."));
        }
        if (m_codeEditor) {
            m_codeEditor->clear();
        }
        return;
    }

    const DiagramDefinition def = m_engine->diagramAt(diagIdx);
    const QVector<DiagramEvent> events = m_engine->eventsForDiagram(diagIdx);

    const int queryIndex = (isTimeTravelMode() && m_selectedDltIndex >= 0) ? m_selectedDltIndex : -1;
    const DiagramSnapshot snapshot = m_engine->computeSnapshotAt(diagIdx, queryIndex);

    updateStatusBanner(def, events, snapshot);

    if (m_graphicsView) {
        m_graphicsView->renderDiagram(def, events, snapshot);
    }

    if (m_timelineTable && snapshot.activeEventIndex >= 0 &&
        snapshot.activeEventIndex < m_timelineTable->rowCount()) {
        m_timelineTable->blockSignals(true);
        m_timelineTable->selectRow(snapshot.activeEventIndex);
        m_timelineTable->scrollToItem(
            m_timelineTable->item(snapshot.activeEventIndex, 0),
            QAbstractItemView::PositionAtCenter);
        m_timelineTable->blockSignals(false);
    }

    if (m_codeEditor) {
        const bool usePlantUml = (m_codeFormatCombo && m_codeFormatCombo->currentIndex() == 1);
        const QString generated = usePlantUml
                                      ? DiagramExporter::generatePlantUml(def, events, snapshot)
                                      : DiagramExporter::generateMermaid(def, events, snapshot);
        m_codeEditor->setPlainText(generated);
    }
}

void Form::updateStatusBanner(const DiagramDefinition &def,
                              const QVector<DiagramEvent> &events,
                              const DiagramSnapshot &snapshot)
{
    if (!m_currentStateBadge) {
        return;
    }

    if (def.type == DiagramType::StateDiagram || def.type == DiagramType::FlowDiagram) {
        const QString kind = (def.type == DiagramType::FlowDiagram)
                                 ? QStringLiteral("Step")
                                 : QStringLiteral("State");
        const QString st = snapshot.currentState.isEmpty() ? QStringLiteral("-") : snapshot.currentState;
        QString detail;
        if (snapshot.currentStateLogIndex >= 0) {
            detail = QStringLiteral(" (#%1)").arg(snapshot.currentStateLogIndex);
        }
        m_currentStateBadge->setText(QStringLiteral("%1: %2%3").arg(kind, st, detail));
    } else {
        if (snapshot.activeEventIndex >= 0 && snapshot.activeEventIndex < events.size()) {
            const DiagramEvent &ev = events[snapshot.activeEventIndex];
            m_currentStateBadge->setText(
                QStringLiteral("Seq %1/%2: %3->%4 (#%5)")
                    .arg(snapshot.activeEventIndex + 1)
                    .arg(events.size())
                    .arg(ev.from, ev.to)
                    .arg(ev.dltIndex));
        } else {
            m_currentStateBadge->setText(QStringLiteral("Seq: -"));
        }
    }
}

void Form::onTimelineRowClicked(int row, int column)
{
    Q_UNUSED(column);
    if (!m_timelineTable) return;
    QTableWidgetItem *item = m_timelineTable->item(row, 0);
    if (!item) return;

    const int dltIndex = item->data(Qt::UserRole).toInt();
    m_selectedDltIndex = dltIndex;
    refreshDiagramAndCodeView();
    emit jumpToLogRequested(dltIndex);
}

void Form::onTimelineCurrentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn)
{
    Q_UNUSED(currentColumn);
    Q_UNUSED(previousColumn);
    if (!m_timelineTable || m_timelineTable->signalsBlocked() || currentRow < 0 || currentRow == previousRow) {
        return;
    }
    onTimelineRowClicked(currentRow, 0);
}

void Form::onApplyScriptClicked()
{
    if (!m_engine || !m_codeEditor) return;

    const int diagIdx = currentDiagramIndex();
    if (diagIdx < 0 || diagIdx >= m_engine->diagramCount()) return;

    DiagramDefinition def = m_engine->diagramAt(diagIdx);
    def.diagramSource = m_codeEditor->toPlainText();
    def.nodes.clear();
    def.edges.clear();
    DiagramParser::parseDiagramSource(def.diagramSource, def);

    QString err;
    m_currentDiagramIdx = m_engine->addOrUpdateDiagram(def, true, err);
    refreshCategoryTree();
    refreshTabJsonEditor();

    if (m_codeStatusLabel) {
        m_codeStatusLabel->setText(
            QStringLiteral("Saved to %1.json (%2 nodes, %3 edges).")
                .arg(def.tabName)
                .arg(def.nodes.size())
                .arg(def.edges.size()));
    }

    emit rescanFileRequested();
}

void Form::onCopyScriptClicked()
{
    if (!m_codeEditor) return;
    if (QClipboard *cb = QApplication::clipboard()) {
        cb->setText(m_codeEditor->toPlainText());
        if (m_codeStatusLabel) {
            m_codeStatusLabel->setText(QStringLiteral("Copied diagram code to clipboard."));
        }
    }
}

void Form::onApplyJsonClicked()
{
    if (!m_jsonEditor || !m_engine) return;
    const QString jsonText = m_jsonEditor->toPlainText();
    const QString activeTab = currentTabName();

    QList<DiagramDefinition> parsed;
    QString err;
    QString parsedTabColor;
    if (!DiagramParser::parseJsonConfig(jsonText, parsed, err, activeTab, &parsedTabColor)) {
        if (m_jsonStatusLabel) {
            m_jsonStatusLabel->setStyleSheet(QStringLiteral("color: #dc2626; font-weight: bold;"));
            m_jsonStatusLabel->setText(err);
        }
        return;
    }

    if (!parsedTabColor.isEmpty() && QColor::isValidColorName(parsedTabColor)) {
        m_engine->setTabColor(activeTab, QColor(parsedTabColor));
    }

    const QList<DiagramDefinition> oldTabDiags = m_engine->diagramsForTab(activeTab);
    for (const DiagramDefinition &oldD : oldTabDiags) {
        m_engine->deleteDiagram(activeTab, oldD.id, false, err);
    }
    for (DiagramDefinition d : parsed) {
        d.tabName = activeTab;
        m_currentDiagramIdx = m_engine->addOrUpdateDiagram(d, false, err);
    }
    m_engine->saveTabToDisk(activeTab, err);

    if (m_jsonStatusLabel) {
        m_jsonStatusLabel->setStyleSheet(QStringLiteral("color: #047857; font-weight: bold;"));
        m_jsonStatusLabel->setText(
            QStringLiteral("Saved %1.json (%2 diagrams) and reloaded.")
                .arg(activeTab)
                .arg(parsed.size()));
    }

    refreshTabBar();
    refreshCategoryTree();
    emit rescanFileRequested();
}

void Form::onLoadSampleJsonClicked()
{
    const QString sample = DiagramParser::defaultSampleJson();
    if (m_jsonEditor) {
        m_jsonEditor->setPlainText(sample);
    }
    onApplyJsonClicked();
}

void Form::onOpenJsonFileClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import Diagram JSON File as Tab"),
        m_engine ? m_engine->workspaceDirectory() : QString(),
        QStringLiteral("JSON / Diagram Files (*.json *.puml *.plantuml *.mmd *.mermaid);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        emit loadConfigFileRequested(filePath);
    }
}

void Form::onSaveJsonFileClicked()
{
    const QString defaultName = currentTabName() + QStringLiteral(".json");
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export Current Tab JSON Configuration"),
        defaultName,
        QStringLiteral("JSON Files (*.json);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        emit saveConfigFileRequested(filePath);
    }
}

void Form::onChooseWorkspaceFolderClicked()
{
    if (!m_engine) return;
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select Folder for <TabName>.json Files"),
        m_engine->workspaceDirectory());
    if (dir.isEmpty()) {
        return;
    }

    m_engine->setWorkspaceDirectory(dir, true);
    onConfigUpdated();
    emit rescanFileRequested();
}

} // namespace DltDiagram
