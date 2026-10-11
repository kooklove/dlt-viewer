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
 * \file diagramengine.h
 * @licence end@
 */

#ifndef DIAGRAMENGINE_H
#define DIAGRAMENGINE_H

#include "diagrammodel.h"
#include "qdltmsg.h"

#include <QMutex>
#include <QVector>
#include <QList>
#include <QSet>
#include <QColor>

namespace DltDiagram {

class DiagramStateEngine {
public:
    DiagramStateEngine();
    ~DiagramStateEngine() = default;

    // --- Single & Multi-JSON / Tab Management ---
    QString workspaceDirectory() const;
    void setWorkspaceDirectory(const QString &dirPath, bool loadExisting = true);

    bool loadTabsFromDirectory(const QString &dirPath, QString &errorOut);
    bool saveTabToDisk(const QString &tabName, QString &errorOut);
    bool saveAllTabsToDisk(QString &errorOut);

    QStringList tabNames() const;
    QStringList generalTabOrder() const;
    bool addTab(const QString &tabName, QString &errorOut);
    bool renameTab(const QString &oldTabName, const QString &newTabName, QString &errorOut);
    bool removeTab(const QString &tabName, bool deleteFileFromDisk, QString &errorOut);
    void setTabOrder(const QStringList &orderedTabNames);

    QColor tabColor(const QString &tabName) const;
    void setTabColor(const QString &tabName, const QColor &color);

    bool isTabFavorite(const QString &tabName) const;
    void setTabFavorite(const QString &tabName, bool favorite);
    bool toggleTabFavorite(const QString &tabName);
    QStringList favoriteTabs() const;

    // --- Diagram CRUD (auto-sorted alphabetically by name per category) ---
    void setDiagrams(const QList<DiagramDefinition> &diagrams);
    QList<DiagramDefinition> diagrams() const;
    QList<DiagramDefinition> diagramsForTab(const QString &tabName) const;
    QList<DiagramDefinition> diagramsForTabAndType(const QString &tabName, DiagramType type) const;

    int diagramCount() const;
    int findDiagramIndex(const QString &tabName, const QString &diagramId) const;
    int findDiagramIndexByFavoriteKey(const QString &favKey) const;
    DiagramDefinition diagramAt(int diagramIdx) const;
    bool updateDiagramDefinition(int diagramIdx, const DiagramDefinition &def);
    int addOrUpdateDiagram(const DiagramDefinition &def, bool saveToFile, QString &errorOut);
    bool deleteDiagram(const QString &tabName, const QString &diagramId, bool saveToFile, QString &errorOut);

    // --- Personal Favorites & Configuration Persistence (QDltSettingsManager) ---
    void loadUserPreferences();
    void saveUserPreferences() const;
    bool isFavorite(const QString &favoriteKey) const;
    void setFavorite(const QString &favoriteKey, bool favorite);
    bool toggleFavorite(const QString &favoriteKey);
    QStringList favoriteKeys() const;
    QList<DiagramDefinition> favoriteDiagramsSorted() const;

    // --- Event Indexing & Time-Travel Snapshot ---
    void clearEvents();
    void processMessage(int dltIndex, QDltMsg &msg);
    void processRawFields(int dltIndex,
                          const QString &timeString,
                          double timestampSec,
                          const QString &ecuid,
                          const QString &apid,
                          const QString &ctid,
                          const QString &payload);

    QVector<DiagramEvent> eventsForDiagram(int diagramIdx) const;
    int eventCountForDiagram(int diagramIdx) const;

    DiagramSnapshot computeSnapshotAt(int diagramIdx, int queryDltIndex) const;

    static QString sanitizeTabFileName(const QString &tabName);

private:
    void rebuildFlatDiagramListLocked();
    bool saveTabToDiskLocked(const QString &tabName, QString &errorOut);

    static QString expandTemplate(const QString &tmpl,
                                  const QRegularExpressionMatch &match,
                                  const QString &ecuid,
                                  const QString &apid,
                                  const QString &ctid);

    static bool matchesIdFilter(const QString &filter, const QString &value);

    mutable QMutex m_mutex;
    QString m_workspaceDir;
    QStringList m_tabOrder;
    QMap<QString, QString> m_tabColors;
    QMap<QString, QString> m_tabFilePaths;
    QList<DiagramDefinition> m_diagrams;
    QVector<QVector<DiagramEvent>> m_eventsPerDiagram;
    QVector<QString> m_runningStatePerDiagram;
    QSet<QString> m_favorites;
    QSet<QString> m_favoriteTabs;
};

} // namespace DltDiagram

#endif // DIAGRAMENGINE_H
