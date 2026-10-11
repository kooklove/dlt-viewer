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
 * \file diagramengine.cpp
 * @licence end@
 */

#include "diagramengine.h"
#include "qdltsettingsmanager.h"

#include <QMutexLocker>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>
#include <limits>

namespace DltDiagram {

namespace {
const QString kSettingsKeyWorkspaceDir = QStringLiteral("plugin/dltdiagramplugin/workspaceDir");
const QString kSettingsKeyTabOrder = QStringLiteral("plugin/dltdiagramplugin/tabOrder");
const QString kSettingsKeyTabColors = QStringLiteral("plugin/dltdiagramplugin/tabColors");
const QString kSettingsKeyFavoriteTabs = QStringLiteral("plugin/dltdiagramplugin/favoriteTabs");
const QString kSettingsKeyFavorites = QStringLiteral("plugin/dltdiagramplugin/favorites");
} // namespace

DiagramStateEngine::DiagramStateEngine()
{
    loadUserPreferences();

    if (m_workspaceDir.isEmpty()) {
        const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        m_workspaceDir = QDir(appData.isEmpty() ? QDir::homePath() : appData)
                             .filePath(QStringLiteral("dlt_diagrams"));
    }

    QDir dir(m_workspaceDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QString err;
    if (!loadTabsFromDirectory(m_workspaceDir, err) || m_diagrams.isEmpty()) {
        QList<DiagramDefinition> defaults;
        if (DiagramParser::parseJsonConfig(DiagramParser::defaultSampleJson(), defaults, err, QStringLiteral("Default"))) {
            setDiagrams(defaults);
            saveTabToDisk(QStringLiteral("Default"), err);
        }
    }
}

QString DiagramStateEngine::sanitizeTabFileName(const QString &tabName)
{
    QString clean = tabName.trimmed();
    if (clean.isEmpty()) {
        clean = QStringLiteral("Tab");
    }
    static const QRegularExpression reInvalid(QStringLiteral("[\\\\/:*?\"<>|]"));
    clean.replace(reInvalid, QStringLiteral("_"));
    return clean;
}

void DiagramStateEngine::loadUserPreferences()
{
    QDltSettingsManager *settings = QDltSettingsManager::getInstance();
    if (!settings) {
        return;
    }

    const QString savedDir = settings->value(kSettingsKeyWorkspaceDir, QString()).toString();
    if (!savedDir.isEmpty()) {
        m_workspaceDir = savedDir;
    }

    const QStringList savedOrder = settings->value(kSettingsKeyTabOrder, QStringList()).toStringList();
    if (!savedOrder.isEmpty()) {
        m_tabOrder = savedOrder;
    }

    const QStringList savedColors = settings->value(kSettingsKeyTabColors, QStringList()).toStringList();
    for (const QString &entry : savedColors) {
        const int sep = entry.lastIndexOf(QStringLiteral("::"));
        if (sep > 0) {
            const QString tName = entry.left(sep);
            const QString cHex = entry.mid(sep + 2).trimmed();
            if (!tName.isEmpty() && QColor::isValidColorName(cHex)) {
                m_tabColors.insert(tName, cHex);
            }
        }
    }

    const QStringList savedFavTabs = settings->value(kSettingsKeyFavoriteTabs, QStringList()).toStringList();
    m_favoriteTabs = QSet<QString>(savedFavTabs.begin(), savedFavTabs.end());

    const QStringList savedFavs = settings->value(kSettingsKeyFavorites, QStringList()).toStringList();
    m_favorites = QSet<QString>(savedFavs.begin(), savedFavs.end());
}

void DiagramStateEngine::saveUserPreferences() const
{
    QDltSettingsManager *settings = QDltSettingsManager::getInstance();
    if (!settings) {
        return;
    }

    settings->setValue(kSettingsKeyWorkspaceDir, m_workspaceDir);
    settings->setValue(kSettingsKeyTabOrder, m_tabOrder);

    QStringList colorEntries;
    for (auto it = m_tabColors.constBegin(); it != m_tabColors.constEnd(); ++it) {
        if (!it.key().isEmpty() && !it.value().isEmpty()) {
            colorEntries.append(it.key() + QStringLiteral("::") + it.value());
        }
    }
    settings->setValue(kSettingsKeyTabColors, colorEntries);
    settings->setValue(kSettingsKeyFavoriteTabs, QStringList(m_favoriteTabs.begin(), m_favoriteTabs.end()));
    settings->setValue(kSettingsKeyFavorites, QStringList(m_favorites.begin(), m_favorites.end()));
}

QString DiagramStateEngine::workspaceDirectory() const
{
    QMutexLocker locker(&m_mutex);
    return m_workspaceDir;
}

void DiagramStateEngine::setWorkspaceDirectory(const QString &dirPath, bool loadExisting)
{
    {
        QMutexLocker locker(&m_mutex);
        m_workspaceDir = dirPath;
        QDir dir(m_workspaceDir);
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }
        saveUserPreferences();
    }
    if (loadExisting) {
        QString err;
        loadTabsFromDirectory(dirPath, err);
    }
}

bool DiagramStateEngine::loadTabsFromDirectory(const QString &dirPath, QString &errorOut)
{
    QMutexLocker locker(&m_mutex);
    errorOut.clear();

    QDir dir(dirPath);
    if (!dir.exists()) {
        errorOut = QStringLiteral("Directory does not exist: %1").arg(dirPath);
        return false;
    }

    m_workspaceDir = dir.absolutePath();
    const QFileInfoList jsonFiles = dir.entryInfoList(
        QStringList{QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name);

    if (jsonFiles.isEmpty()) {
        return false;
    }

    QList<DiagramDefinition> loadedAll;
    QStringList discoveredTabs;
    m_tabFilePaths.clear();

    for (const QFileInfo &fi : jsonFiles) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString jsonText = QString::fromUtf8(f.readAll());
        f.close();

        const QString tabName = fi.completeBaseName();
        QList<DiagramDefinition> tabDiags;
        QString parseErr;
        QString fileTabColor;
        if (DiagramParser::parseJsonConfig(jsonText, tabDiags, parseErr, tabName, &fileTabColor)) {
            for (DiagramDefinition &d : tabDiags) {
                d.tabName = tabName;
            }
            if (!fileTabColor.isEmpty() && QColor::isValidColorName(fileTabColor) && !m_tabColors.contains(tabName)) {
                m_tabColors.insert(tabName, fileTabColor);
            }
            DiagramParser::sortDiagramsAlphabetically(tabDiags);
            loadedAll.append(tabDiags);
            if (!discoveredTabs.contains(tabName)) {
                discoveredTabs.append(tabName);
            }
            m_tabFilePaths.insert(tabName, fi.absoluteFilePath());
        } else {
            errorOut = parseErr;
        }
    }

    if (discoveredTabs.isEmpty()) {
        return false;
    }

    // Merge discoveredTabs into m_tabOrder preserving user's saved ordering
    QStringList finalOrder;
    for (const QString &t : m_tabOrder) {
        if (discoveredTabs.contains(t) && !finalOrder.contains(t)) {
            finalOrder.append(t);
        }
    }
    for (const QString &t : discoveredTabs) {
        if (!finalOrder.contains(t)) {
            finalOrder.append(t);
        }
    }
    m_tabOrder = finalOrder;

    m_diagrams = loadedAll;
    rebuildFlatDiagramListLocked();
    saveUserPreferences();
    return true;
}

void DiagramStateEngine::rebuildFlatDiagramListLocked()
{
    // Sort m_diagrams by: 1) Tab Order in m_tabOrder, 2) DiagramType (State, Sequence, Flow), 3) Alphabetical Title
    std::stable_sort(m_diagrams.begin(), m_diagrams.end(),
                     [this](const DiagramDefinition &a, const DiagramDefinition &b) {
                         if (a.tabName != b.tabName) {
                             int idxA = m_tabOrder.indexOf(a.tabName);
                             int idxB = m_tabOrder.indexOf(b.tabName);
                             if (idxA < 0) idxA = 999999;
                             if (idxB < 0) idxB = 999999;
                             if (idxA != idxB) return idxA < idxB;
                             return QString::compare(a.tabName, b.tabName, Qt::CaseInsensitive) < 0;
                         }
                         if (a.type != b.type) {
                             return static_cast<int>(a.type) < static_cast<int>(b.type);
                         }
                         const int cmp = QString::compare(a.title, b.title, Qt::CaseInsensitive);
                         if (cmp != 0) return cmp < 0;
                         return QString::compare(a.id, b.id, Qt::CaseInsensitive) < 0;
                     });

    m_eventsPerDiagram.clear();
    m_eventsPerDiagram.resize(m_diagrams.size());
    m_runningStatePerDiagram.clear();
    m_runningStatePerDiagram.resize(m_diagrams.size());
    for (int i = 0; i < m_diagrams.size(); ++i) {
        m_runningStatePerDiagram[i] = m_diagrams[i].initialState;
    }
}

bool DiagramStateEngine::saveTabToDiskLocked(const QString &tabName, QString &errorOut)
{
    if (m_workspaceDir.isEmpty()) {
        errorOut = QStringLiteral("Workspace directory is not set.");
        return false;
    }

    QDir dir(m_workspaceDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QString filePath = m_tabFilePaths.value(tabName);
    if (filePath.isEmpty()) {
        filePath = dir.filePath(sanitizeTabFileName(tabName) + QStringLiteral(".json"));
        m_tabFilePaths.insert(tabName, filePath);
    }

    QList<DiagramDefinition> tabDiags;
    for (const DiagramDefinition &d : m_diagrams) {
        if (d.tabName == tabName) {
            tabDiags.append(d);
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorOut = QStringLiteral("Cannot write tab JSON file: %1").arg(filePath);
        return false;
    }

    const QString jsonText = DiagramParser::toJsonConfig(tabDiags, tabName, m_tabColors.value(tabName));
    file.write(jsonText.toUtf8());
    file.close();
    return true;
}

bool DiagramStateEngine::saveTabToDisk(const QString &tabName, QString &errorOut)
{
    QMutexLocker locker(&m_mutex);
    return saveTabToDiskLocked(tabName, errorOut);
}

bool DiagramStateEngine::saveAllTabsToDisk(QString &errorOut)
{
    QMutexLocker locker(&m_mutex);
    for (const QString &tab : m_tabOrder) {
        if (!saveTabToDiskLocked(tab, errorOut)) {
            return false;
        }
    }
    return true;
}

QStringList DiagramStateEngine::tabNames() const
{
    QMutexLocker locker(&m_mutex);
    QStringList favFirst;
    QStringList rest;
    for (const QString &t : m_tabOrder) {
        if (m_favoriteTabs.contains(t)) {
            favFirst.append(t);
        } else {
            rest.append(t);
        }
    }
    return favFirst + rest;
}

QStringList DiagramStateEngine::generalTabOrder() const
{
    QMutexLocker locker(&m_mutex);
    return m_tabOrder;
}

bool DiagramStateEngine::isTabFavorite(const QString &tabName) const
{
    QMutexLocker locker(&m_mutex);
    return m_favoriteTabs.contains(tabName.trimmed());
}

void DiagramStateEngine::setTabFavorite(const QString &tabName, bool favorite)
{
    QMutexLocker locker(&m_mutex);
    const QString trimmed = tabName.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (favorite) {
        m_favoriteTabs.insert(trimmed);
    } else {
        m_favoriteTabs.remove(trimmed);
    }
    saveUserPreferences();
}

bool DiagramStateEngine::toggleTabFavorite(const QString &tabName)
{
    QMutexLocker locker(&m_mutex);
    const QString trimmed = tabName.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    bool nowFav = false;
    if (m_favoriteTabs.contains(trimmed)) {
        m_favoriteTabs.remove(trimmed);
        nowFav = false;
    } else {
        m_favoriteTabs.insert(trimmed);
        nowFav = true;
    }
    saveUserPreferences();
    return nowFav;
}

QStringList DiagramStateEngine::favoriteTabs() const
{
    QMutexLocker locker(&m_mutex);
    QStringList favs;
    for (const QString &t : m_tabOrder) {
        if (m_favoriteTabs.contains(t)) {
            favs.append(t);
        }
    }
    return favs;
}

QColor DiagramStateEngine::tabColor(const QString &tabName) const
{
    QMutexLocker locker(&m_mutex);
    const QString hex = m_tabColors.value(tabName);
    if (!hex.isEmpty() && QColor::isValidColorName(hex)) {
        return QColor(hex);
    }
    return QColor();
}

void DiagramStateEngine::setTabColor(const QString &tabName, const QColor &color)
{
    QMutexLocker locker(&m_mutex);
    const QString trimmed = tabName.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (color.isValid()) {
        m_tabColors.insert(trimmed, color.name(QColor::HexRgb));
    } else {
        m_tabColors.remove(trimmed);
    }
    QString err;
    saveTabToDiskLocked(trimmed, err);
    saveUserPreferences();
}

bool DiagramStateEngine::addTab(const QString &tabName, QString &errorOut)
{
    QMutexLocker locker(&m_mutex);
    const QString trimmed = tabName.trimmed();
    if (trimmed.isEmpty()) {
        errorOut = QStringLiteral("Tab name cannot be empty.");
        return false;
    }
    if (m_tabOrder.contains(trimmed)) {
        errorOut = QStringLiteral("Tab '%1' already exists.").arg(trimmed);
        return false;
    }

    m_tabOrder.append(trimmed);
    const bool saved = saveTabToDiskLocked(trimmed, errorOut);
    saveUserPreferences();
    return saved;
}

bool DiagramStateEngine::renameTab(const QString &oldTabName, const QString &newTabName, QString &errorOut)
{
    QMutexLocker locker(&m_mutex);
    const QString trimmedNew = newTabName.trimmed();
    if (trimmedNew.isEmpty()) {
        errorOut = QStringLiteral("New tab name cannot be empty.");
        return false;
    }
    if (oldTabName == trimmedNew) {
        return true;
    }
    if (m_tabOrder.contains(trimmedNew)) {
        errorOut = QStringLiteral("Tab '%1' already exists.").arg(trimmedNew);
        return false;
    }

    const int idx = m_tabOrder.indexOf(oldTabName);
    if (idx < 0) {
        errorOut = QStringLiteral("Tab '%1' not found.").arg(oldTabName);
        return false;
    }

    m_tabOrder[idx] = trimmedNew;
    if (m_tabColors.contains(oldTabName)) {
        m_tabColors.insert(trimmedNew, m_tabColors.take(oldTabName));
    }
    if (m_favoriteTabs.remove(oldTabName)) {
        m_favoriteTabs.insert(trimmedNew);
    }
    for (DiagramDefinition &d : m_diagrams) {
        if (d.tabName == oldTabName) {
            const QString oldFavKey = d.favoriteKey();
            d.tabName = trimmedNew;
            if (m_favorites.remove(oldFavKey)) {
                m_favorites.insert(d.favoriteKey());
            }
        }
    }

    const QString oldPath = m_tabFilePaths.take(oldTabName);
    if (!oldPath.isEmpty() && QFile::exists(oldPath)) {
        QFile::remove(oldPath);
    }

    const bool ok = saveTabToDiskLocked(trimmedNew, errorOut);
    saveUserPreferences();
    return ok;
}

bool DiagramStateEngine::removeTab(const QString &tabName, bool deleteFileFromDisk, QString &errorOut)
{
    QMutexLocker locker(&m_mutex);
    if (!m_tabOrder.contains(tabName)) {
        errorOut = QStringLiteral("Tab '%1' not found.").arg(tabName);
        return false;
    }

    m_tabOrder.removeAll(tabName);
    m_tabColors.remove(tabName);
    m_favoriteTabs.remove(tabName);
    for (int i = m_diagrams.size() - 1; i >= 0; --i) {
        if (m_diagrams[i].tabName == tabName) {
            m_favorites.remove(m_diagrams[i].favoriteKey());
            m_diagrams.removeAt(i);
        }
    }

    const QString oldPath = m_tabFilePaths.take(tabName);
    if (deleteFileFromDisk && !oldPath.isEmpty() && QFile::exists(oldPath)) {
        QFile::remove(oldPath);
    }

    if (m_tabOrder.isEmpty()) {
        m_tabOrder.append(QStringLiteral("Default"));
        saveTabToDiskLocked(QStringLiteral("Default"), errorOut);
    }

    rebuildFlatDiagramListLocked();
    saveUserPreferences();
    return true;
}

void DiagramStateEngine::setTabOrder(const QStringList &orderedTabNames)
{
    QMutexLocker locker(&m_mutex);
    QStringList validOrder;
    for (const QString &name : orderedTabNames) {
        if (m_tabOrder.contains(name) && !validOrder.contains(name)) {
            validOrder.append(name);
        }
    }
    for (const QString &existing : m_tabOrder) {
        if (!validOrder.contains(existing)) {
            validOrder.append(existing);
        }
    }
    m_tabOrder = validOrder;
    rebuildFlatDiagramListLocked();
    saveUserPreferences();
}

void DiagramStateEngine::setDiagrams(const QList<DiagramDefinition> &diagrams)
{
    QMutexLocker locker(&m_mutex);
    m_diagrams = diagrams;

    for (const DiagramDefinition &d : m_diagrams) {
        const QString tName = d.tabName.isEmpty() ? QStringLiteral("Default") : d.tabName;
        if (!m_tabOrder.contains(tName)) {
            m_tabOrder.append(tName);
        }
    }
    if (m_tabOrder.isEmpty()) {
        m_tabOrder.append(QStringLiteral("Default"));
    }

    rebuildFlatDiagramListLocked();
}

QList<DiagramDefinition> DiagramStateEngine::diagrams() const
{
    QMutexLocker locker(&m_mutex);
    return m_diagrams;
}

QList<DiagramDefinition> DiagramStateEngine::diagramsForTab(const QString &tabName) const
{
    QMutexLocker locker(&m_mutex);
    QList<DiagramDefinition> list;
    for (const DiagramDefinition &d : m_diagrams) {
        if (d.tabName == tabName) {
            list.append(d);
        }
    }
    DiagramParser::sortDiagramsAlphabetically(list);
    return list;
}

QList<DiagramDefinition> DiagramStateEngine::diagramsForTabAndType(const QString &tabName, DiagramType type) const
{
    QMutexLocker locker(&m_mutex);
    QList<DiagramDefinition> list;
    for (const DiagramDefinition &d : m_diagrams) {
        if (d.tabName == tabName && d.type == type) {
            list.append(d);
        }
    }
    DiagramParser::sortDiagramsAlphabetically(list);
    return list;
}

int DiagramStateEngine::diagramCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_diagrams.size();
}

int DiagramStateEngine::findDiagramIndex(const QString &tabName, const QString &diagramId) const
{
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < m_diagrams.size(); ++i) {
        if (m_diagrams[i].tabName == tabName && m_diagrams[i].id == diagramId) {
            return i;
        }
    }
    return -1;
}

int DiagramStateEngine::findDiagramIndexByFavoriteKey(const QString &favKey) const
{
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < m_diagrams.size(); ++i) {
        if (m_diagrams[i].favoriteKey() == favKey) {
            return i;
        }
    }
    return -1;
}

DiagramDefinition DiagramStateEngine::diagramAt(int diagramIdx) const
{
    QMutexLocker locker(&m_mutex);
    if (diagramIdx >= 0 && diagramIdx < m_diagrams.size()) {
        return m_diagrams[diagramIdx];
    }
    return DiagramDefinition();
}

bool DiagramStateEngine::updateDiagramDefinition(int diagramIdx, const DiagramDefinition &def)
{
    QMutexLocker locker(&m_mutex);
    if (diagramIdx < 0 || diagramIdx >= m_diagrams.size()) {
        return false;
    }
    m_diagrams[diagramIdx] = def;
    rebuildFlatDiagramListLocked();
    QString err;
    saveTabToDiskLocked(def.tabName, err);
    return true;
}

int DiagramStateEngine::addOrUpdateDiagram(const DiagramDefinition &def, bool saveToFile, QString &errorOut)
{
    QMutexLocker locker(&m_mutex);
    DiagramDefinition cleanDef = def;
    if (cleanDef.tabName.trimmed().isEmpty()) {
        cleanDef.tabName = m_tabOrder.isEmpty() ? QStringLiteral("Default") : m_tabOrder.first();
    }
    if (cleanDef.id.trimmed().isEmpty()) {
        cleanDef.id = sanitizeTabFileName(cleanDef.title).toLower();
    }
    if (!m_tabOrder.contains(cleanDef.tabName)) {
        m_tabOrder.append(cleanDef.tabName);
    }

    bool replaced = false;
    for (int i = 0; i < m_diagrams.size(); ++i) {
        if (m_diagrams[i].tabName == cleanDef.tabName && m_diagrams[i].id == cleanDef.id) {
            m_diagrams[i] = cleanDef;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_diagrams.append(cleanDef);
    }

    rebuildFlatDiagramListLocked();
    if (saveToFile) {
        saveTabToDiskLocked(cleanDef.tabName, errorOut);
        saveUserPreferences();
    }

    for (int i = 0; i < m_diagrams.size(); ++i) {
        if (m_diagrams[i].tabName == cleanDef.tabName && m_diagrams[i].id == cleanDef.id) {
            return i;
        }
    }
    return 0;
}

bool DiagramStateEngine::deleteDiagram(const QString &tabName, const QString &diagramId, bool saveToFile, QString &errorOut)
{
    QMutexLocker locker(&m_mutex);
    bool removed = false;
    for (int i = m_diagrams.size() - 1; i >= 0; --i) {
        if (m_diagrams[i].tabName == tabName && m_diagrams[i].id == diagramId) {
            m_favorites.remove(m_diagrams[i].favoriteKey());
            m_diagrams.removeAt(i);
            removed = true;
            break;
        }
    }
    if (!removed) {
        errorOut = QStringLiteral("Diagram '%1' not found in tab '%2'.").arg(diagramId, tabName);
        return false;
    }

    rebuildFlatDiagramListLocked();
    if (saveToFile) {
        saveTabToDiskLocked(tabName, errorOut);
        saveUserPreferences();
    }
    return true;
}

bool DiagramStateEngine::isFavorite(const QString &favoriteKey) const
{
    QMutexLocker locker(&m_mutex);
    return m_favorites.contains(favoriteKey);
}

void DiagramStateEngine::setFavorite(const QString &favoriteKey, bool favorite)
{
    QMutexLocker locker(&m_mutex);
    if (favorite) {
        m_favorites.insert(favoriteKey);
    } else {
        m_favorites.remove(favoriteKey);
    }
    saveUserPreferences();
}

bool DiagramStateEngine::toggleFavorite(const QString &favoriteKey)
{
    QMutexLocker locker(&m_mutex);
    bool nowFav = false;
    if (m_favorites.contains(favoriteKey)) {
        m_favorites.remove(favoriteKey);
        nowFav = false;
    } else {
        m_favorites.insert(favoriteKey);
        nowFav = true;
    }
    saveUserPreferences();
    return nowFav;
}

QStringList DiagramStateEngine::favoriteKeys() const
{
    QMutexLocker locker(&m_mutex);
    QStringList list(m_favorites.begin(), m_favorites.end());
    list.sort(Qt::CaseInsensitive);
    return list;
}

QList<DiagramDefinition> DiagramStateEngine::favoriteDiagramsSorted() const
{
    QMutexLocker locker(&m_mutex);
    QList<DiagramDefinition> favs;
    for (const DiagramDefinition &d : m_diagrams) {
        if (m_favorites.contains(d.favoriteKey())) {
            favs.append(d);
        }
    }
    std::stable_sort(favs.begin(), favs.end(),
                     [](const DiagramDefinition &a, const DiagramDefinition &b) {
                         const int cmp = QString::compare(a.title, b.title, Qt::CaseInsensitive);
                         if (cmp != 0) return cmp < 0;
                         return QString::compare(a.tabName, b.tabName, Qt::CaseInsensitive) < 0;
                     });
    return favs;
}

void DiagramStateEngine::clearEvents()
{
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < m_eventsPerDiagram.size(); ++i) {
        m_eventsPerDiagram[i].clear();
        m_runningStatePerDiagram[i] = (i < m_diagrams.size()) ? m_diagrams[i].initialState : QString();
    }
}

bool DiagramStateEngine::matchesIdFilter(const QString &filter, const QString &value)
{
    const QString f = filter.trimmed();
    if (f.isEmpty() || f == QStringLiteral("*")) {
        return true;
    }
    if (f.contains(',')) {
        const QStringList parts = f.split(',', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            if (part.trimmed().compare(value.trimmed(), Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    }
    return f.compare(value.trimmed(), Qt::CaseInsensitive) == 0;
}

QString DiagramStateEngine::expandTemplate(const QString &tmpl,
                                           const QRegularExpressionMatch &match,
                                           const QString &ecuid,
                                           const QString &apid,
                                           const QString &ctid)
{
    if (tmpl.isEmpty()) {
        return QString();
    }

    QString result = tmpl;
    result.replace(QStringLiteral("$ECUID"), ecuid.trimmed().isEmpty() ? QStringLiteral("ECU") : ecuid.trimmed());
    result.replace(QStringLiteral("$APID"), apid.trimmed().isEmpty() ? QStringLiteral("APP") : apid.trimmed());
    result.replace(QStringLiteral("$CTID"), ctid.trimmed().isEmpty() ? QStringLiteral("CTX") : ctid.trimmed());

    for (int group = 9; group >= 1; --group) {
        const QString token = QStringLiteral("$%1").arg(group);
        if (result.contains(token)) {
            const QString captured = (group <= match.lastCapturedIndex()) ? match.captured(group).trimmed() : QString();
            result.replace(token, captured);
        }
    }

    return result.trimmed();
}

void DiagramStateEngine::processMessage(int dltIndex, QDltMsg &msg)
{
    const QString timeStr = QStringLiteral("%1.%2")
                                .arg(msg.getTimeString())
                                .arg(msg.getMicroseconds(), 6, 10, QLatin1Char('0'));
    const double timestampSec = static_cast<double>(msg.getTimestamp()) / 10000.0;
    const QString ecuid = msg.getEcuid();
    const QString apid = msg.getApid();
    const QString ctid = msg.getCtid();
    const QString payload = msg.toStringPayload();

    processRawFields(dltIndex, timeStr, timestampSec, ecuid, apid, ctid, payload);
}

void DiagramStateEngine::processRawFields(int dltIndex,
                                          const QString &timeString,
                                          double timestampSec,
                                          const QString &ecuid,
                                          const QString &apid,
                                          const QString &ctid,
                                          const QString &payload)
{
    QMutexLocker locker(&m_mutex);
    if (m_diagrams.isEmpty()) {
        return;
    }

    for (int dIdx = 0; dIdx < m_diagrams.size(); ++dIdx) {
        const DiagramDefinition &def = m_diagrams[dIdx];
        QVector<DiagramEvent> &events = m_eventsPerDiagram[dIdx];

        if (!events.isEmpty() && events.last().dltIndex == dltIndex) {
            continue;
        }

        for (const DiagramRule &rule : def.rules) {
            if (!matchesIdFilter(rule.ecuid, ecuid) ||
                !matchesIdFilter(rule.apid, apid) ||
                !matchesIdFilter(rule.ctid, ctid)) {
                continue;
            }

            if (rule.payloadRegexString.isEmpty() || !rule.payloadRegex.isValid()) {
                continue;
            }

            const QRegularExpressionMatch match = rule.payloadRegex.match(payload);
            if (!match.hasMatch()) {
                continue;
            }

            QString from = expandTemplate(rule.fromTemplate, match, ecuid, apid, ctid);
            QString to = expandTemplate(rule.toTemplate, match, ecuid, apid, ctid);
            QString label = expandTemplate(rule.labelTemplate, match, ecuid, apid, ctid);

            // In State or Flow diagrams, if 'to' is empty because an optional second group didn't match,
            // allow 'from' to act as the target step/state!
            if (to.isEmpty() && !from.isEmpty() && from != QStringLiteral("*") &&
                def.type != DiagramType::SequenceDiagram) {
                to = from;
                from = QStringLiteral("*");
            }

            if (to.isEmpty()) {
                continue;
            }

            if (def.type == DiagramType::StateDiagram || def.type == DiagramType::FlowDiagram) {
                if (from.isEmpty() || from == QStringLiteral("*")) {
                    from = m_runningStatePerDiagram[dIdx];
                    if (from.isEmpty()) {
                        from = def.initialState.isEmpty() ? to : def.initialState;
                    }
                }
                m_runningStatePerDiagram[dIdx] = to;
            } else {
                if (from.isEmpty() || from == QStringLiteral("*")) {
                    from = apid.trimmed().isEmpty() ? QStringLiteral("System") : apid.trimmed();
                }
            }

            if (label.isEmpty()) {
                label = (def.type != DiagramType::SequenceDiagram)
                            ? QStringLiteral("%1 -> %2").arg(from, to)
                            : rule.name;
            }

            DiagramEvent ev;
            ev.dltIndex = dltIndex;
            ev.timeString = timeString;
            ev.timestampSec = timestampSec;
            ev.ecuid = ecuid;
            ev.apid = apid;
            ev.ctid = ctid;
            ev.from = from;
            ev.to = to;
            ev.label = label;
            ev.arrowStyle = rule.arrowStyle;
            ev.ruleName = rule.name;
            ev.payload = payload;

            if (events.isEmpty() || events.last().dltIndex < dltIndex) {
                events.append(ev);
            } else {
                auto it = std::lower_bound(events.begin(), events.end(), dltIndex,
                                           [](const DiagramEvent &e, int idx) { return e.dltIndex < idx; });
                if (it != events.end() && it->dltIndex == dltIndex) {
                    *it = ev;
                } else {
                    events.insert(it, ev);
                }
            }
            break;
        }
    }
}

QVector<DiagramEvent> DiagramStateEngine::eventsForDiagram(int diagramIdx) const
{
    QMutexLocker locker(&m_mutex);
    if (diagramIdx >= 0 && diagramIdx < m_eventsPerDiagram.size()) {
        return m_eventsPerDiagram[diagramIdx];
    }
    return QVector<DiagramEvent>();
}

int DiagramStateEngine::eventCountForDiagram(int diagramIdx) const
{
    QMutexLocker locker(&m_mutex);
    if (diagramIdx >= 0 && diagramIdx < m_eventsPerDiagram.size()) {
        return m_eventsPerDiagram[diagramIdx].size();
    }
    return 0;
}

DiagramSnapshot DiagramStateEngine::computeSnapshotAt(int diagramIdx, int queryDltIndex) const
{
    QMutexLocker locker(&m_mutex);
    DiagramSnapshot snap;
    snap.queryDltIndex = queryDltIndex;

    if (diagramIdx < 0 || diagramIdx >= m_diagrams.size()) {
        return snap;
    }

    const DiagramDefinition &def = m_diagrams[diagramIdx];
    const QVector<DiagramEvent> &events = m_eventsPerDiagram[diagramIdx];

    if (!def.initialState.isEmpty()) {
        snap.currentState = def.initialState;
        snap.stateVisitCounts[def.initialState] = 1;
        snap.stateLastLogIndex[def.initialState] = -1;
    }

    if (events.isEmpty()) {
        return snap;
    }

    const int effectiveQueryIdx = (queryDltIndex < 0) ? std::numeric_limits<int>::max() : queryDltIndex;

    auto upperIt = std::upper_bound(events.begin(), events.end(), effectiveQueryIdx,
                                    [](int idx, const DiagramEvent &ev) { return idx < ev.dltIndex; });
    const int endPos = static_cast<int>(std::distance(events.begin(), upperIt));
    if (endPos <= 0) {
        return snap;
    }

    snap.activeEventIndex = endPos - 1;
    snap.exactMatchAtIndex = (events[snap.activeEventIndex].dltIndex == queryDltIndex);

    for (int i = 0; i < endPos; ++i) {
        const DiagramEvent &ev = events[i];
        if (!ev.from.isEmpty() && !snap.stateVisitCounts.contains(ev.from)) {
            snap.stateVisitCounts[ev.from] = 1;
            snap.stateLastLogIndex[ev.from] = ev.dltIndex;
            snap.stateLastTime[ev.from] = ev.timeString;
        }
        if (!ev.to.isEmpty()) {
            snap.stateVisitCounts[ev.to] = snap.stateVisitCounts.value(ev.to, 0) + 1;
            snap.stateLastLogIndex[ev.to] = ev.dltIndex;
            snap.stateLastTime[ev.to] = ev.timeString;
        }
        if (!ev.from.isEmpty() && !ev.to.isEmpty()) {
            const QPair<QString, QString> edgeKey(ev.from, ev.to);
            snap.transitionCounts[edgeKey] = snap.transitionCounts.value(edgeKey, 0) + 1;
            snap.transitionLastLogIndex[edgeKey] = ev.dltIndex;
            snap.transitionLastLabel[edgeKey] = ev.label;
        }
    }

    const DiagramEvent &lastEv = events[snap.activeEventIndex];
    snap.currentState = lastEv.to;
    snap.currentStateLogIndex = lastEv.dltIndex;
    snap.currentStateTime = lastEv.timeString;

    return snap;
}

} // namespace DltDiagram
