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
 * \file dltdiagramplugin.cpp
 * @licence end@
 */

#include "dltdiagramplugin.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QMetaObject>

DltDiagramPlugin::DltDiagramPlugin()
{
    m_rawJsonConfig = DltDiagram::DiagramParser::defaultSampleJson();
}

DltDiagramPlugin::~DltDiagramPlugin() = default;

QString DltDiagramPlugin::name()
{
    return QStringLiteral("DLT Diagram Plugin");
}

QString DltDiagramPlugin::pluginVersion()
{
    return QStringLiteral(DLT_DIAGRAM_PLUGIN_VERSION);
}

QString DltDiagramPlugin::pluginInterfaceVersion()
{
    return QStringLiteral(PLUGIN_INTERFACE_VERSION);
}

QString DltDiagramPlugin::description()
{
    return QStringLiteral(
        "Visualizes State Diagrams, Sequence Diagrams, and Flow Diagrams across multiple <TabName>.json tabs "
        "with a built-in GUI Builder, personal Favorites, and live/time-travel DLT log synchronization.");
}

QString DltDiagramPlugin::error()
{
    return m_errorText;
}

bool DltDiagramPlugin::applyJsonString(const QString &jsonText)
{
    const QString activeTab = m_form ? m_form->currentTabName() : QStringLiteral("Default");
    QList<DltDiagram::DiagramDefinition> parsed;
    QString parseErr;
    if (!DltDiagram::DiagramParser::parseJsonConfig(jsonText, parsed, parseErr, activeTab)) {
        m_errorText = parseErr;
        return false;
    }

    m_errorText.clear();
    m_rawJsonConfig = jsonText;

    for (DltDiagram::DiagramDefinition &d : parsed) {
        if (d.tabName.isEmpty()) {
            d.tabName = activeTab;
        }
        m_engine.addOrUpdateDiagram(d, true, parseErr);
    }

    if (m_form) {
        m_form->onConfigUpdated(m_rawJsonConfig);
    }
    return true;
}

bool DltDiagramPlugin::loadConfig(QString filename)
{
    if (filename.trimmed().isEmpty()) {
        QString err;
        if (!m_engine.loadTabsFromDirectory(m_engine.workspaceDirectory(), err) || m_engine.diagramCount() == 0) {
            m_rawJsonConfig = DltDiagram::DiagramParser::defaultSampleJson();
            applyJsonString(m_rawJsonConfig);
        }
        if (m_form) {
            m_form->onConfigUpdated();
        }
        if (m_dltFile && m_dltFile->size() > 0) {
            rescanLoadedFile();
        }
        return true;
    }

    QFileInfo fi(filename);
    if (fi.isDir()) {
        QString err;
        const bool ok = m_engine.loadTabsFromDirectory(fi.absoluteFilePath(), err);
        if (!ok) {
            m_errorText = err;
            return false;
        }
        if (m_form) {
            m_form->onConfigUpdated();
        }
        if (m_dltFile && m_dltFile->size() > 0) {
            rescanLoadedFile();
        }
        return true;
    }

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_errorText = QStringLiteral("Cannot open configuration file: %1").arg(filename);
        return false;
    }

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    const QString tabNameFromFile = fi.completeBaseName().isEmpty() ? QStringLiteral("Default") : fi.completeBaseName();
    const QString suffix = fi.suffix().toLower();

    if (suffix == QStringLiteral("puml") || suffix == QStringLiteral("plantuml") ||
        suffix == QStringLiteral("mmd") || suffix == QStringLiteral("mermaid") ||
        content.trimmed().startsWith(QStringLiteral("@startuml"), Qt::CaseInsensitive) ||
        content.trimmed().startsWith(QStringLiteral("stateDiagram"), Qt::CaseInsensitive) ||
        content.trimmed().startsWith(QStringLiteral("sequenceDiagram"), Qt::CaseInsensitive) ||
        content.trimmed().startsWith(QStringLiteral("flowchart "), Qt::CaseInsensitive) ||
        content.trimmed().startsWith(QStringLiteral("graph "), Qt::CaseInsensitive)) {
        DltDiagram::DiagramDefinition def;
        def.tabName = tabNameFromFile;
        def.id = tabNameFromFile.toLower();
        def.title = tabNameFromFile;
        def.diagramSource = content;
        DltDiagram::DiagramParser::parseDiagramSource(content, def);

        if (def.rules.isEmpty()) {
            DltDiagram::DiagramRule rule;
            rule.name = QStringLiteral("State/Sequence/Flow Arrow Pattern");
            rule.payloadRegexString = QStringLiteral("([A-Za-z0-9_]+)\\s*(?:->|-->|=>)\\s*([A-Za-z0-9_]+)(?:\\s*:\\s*(.+))?");
            rule.payloadRegex = QRegularExpression(rule.payloadRegexString);
            rule.fromTemplate = QStringLiteral("$1");
            rule.toTemplate = QStringLiteral("$2");
            rule.labelTemplate = QStringLiteral("$3");
            def.rules.append(rule);
        }

        QString err;
        m_engine.addOrUpdateDiagram(def, true, err);
        if (m_form) {
            m_form->onConfigUpdated();
        }
        if (m_dltFile && m_dltFile->size() > 0) {
            rescanLoadedFile();
        }
        return true;
    }

    QList<DltDiagram::DiagramDefinition> parsed;
    QString parseErr;
    if (!DltDiagram::DiagramParser::parseJsonConfig(content, parsed, parseErr, tabNameFromFile)) {
        m_errorText = parseErr;
        return false;
    }

    QString saveErr;
    for (DltDiagram::DiagramDefinition &d : parsed) {
        d.tabName = tabNameFromFile;
        m_engine.addOrUpdateDiagram(d, false, saveErr);
    }
    m_engine.saveTabToDisk(tabNameFromFile, saveErr);

    if (m_form) {
        m_form->onConfigUpdated();
    }
    if (m_dltFile && m_dltFile->size() > 0) {
        rescanLoadedFile();
    }
    return true;
}

bool DltDiagramPlugin::saveConfig(QString filename)
{
    m_engine.saveUserPreferences();

    if (filename.trimmed().isEmpty()) {
        QString err;
        return m_engine.saveAllTabsToDisk(err);
    }

    QFileInfo fi(filename);
    if (fi.isDir()) {
        m_engine.setWorkspaceDirectory(fi.absoluteFilePath(), false);
        QString err;
        return m_engine.saveAllTabsToDisk(err);
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_errorText = QStringLiteral("Cannot write configuration file: %1").arg(filename);
        return false;
    }

    const QString activeTab = m_form ? m_form->currentTabName() : QString();
    const QList<DltDiagram::DiagramDefinition> diags =
        activeTab.isEmpty() ? m_engine.diagrams() : m_engine.diagramsForTab(activeTab);
    const QString jsonOut = DltDiagram::DiagramParser::toJsonConfig(diags, activeTab);
    file.write(jsonOut.toUtf8());
    file.close();
    return true;
}

QStringList DltDiagramPlugin::infoConfig()
{
    QStringList info;
    info.append(QStringLiteral("Workspace: %1").arg(m_engine.workspaceDirectory()));
    info.append(QStringLiteral("Tabs (%1): %2").arg(m_engine.tabNames().size()).arg(m_engine.tabNames().join(QStringLiteral(", "))));
    info.append(QStringLiteral("Favorites (%1): %2").arg(m_engine.favoriteKeys().size()).arg(m_engine.favoriteKeys().join(QStringLiteral(", "))));

    const QList<DltDiagram::DiagramDefinition> list = m_engine.diagrams();
    for (int i = 0; i < list.size(); ++i) {
        const auto &def = list[i];
        const QString typeStr = DltDiagram::DiagramDefinition::typeToString(def.type);
        info.append(QStringLiteral("[%1.json] %2 (%3): %4 rules, %5 nodes, %6 matched events")
                        .arg(def.tabName, def.title, typeStr)
                        .arg(def.rules.size())
                        .arg(def.nodes.size())
                        .arg(m_engine.eventCountForDiagram(i)));
    }
    return info;
}

QWidget* DltDiagramPlugin::initViewer()
{
    if (!m_form) {
        m_form = new DltDiagram::Form(&m_engine);
        m_form->onConfigUpdated();

        connect(m_form, &DltDiagram::Form::jumpToLogRequested,
                this, &DltDiagramPlugin::onJumpToLogRequested);
        connect(m_form, &DltDiagram::Form::applyJsonConfigRequested,
                this, &DltDiagramPlugin::onApplyJsonConfigRequested);
        connect(m_form, &DltDiagram::Form::rescanFileRequested,
                this, &DltDiagramPlugin::rescanLoadedFile);
        connect(m_form, &DltDiagram::Form::loadConfigFileRequested,
                this, [this](const QString &path) { loadConfig(path); });
        connect(m_form, &DltDiagram::Form::saveConfigFileRequested,
                this, [this](const QString &path) { saveConfig(path); });
    }
    return m_form;
}

void DltDiagramPlugin::initFileStart(QDltFile *file)
{
    m_dltFile = file;
    m_lastUpdatedIndex = -1;
    m_engine.clearEvents();
}

void DltDiagramPlugin::initMsg(int /*index*/, QDltMsg &/*msg*/)
{
}

void DltDiagramPlugin::initMsgDecoded(int index, QDltMsg &msg)
{
    m_engine.processMessage(index, msg);
    m_lastUpdatedIndex = index;
}

void DltDiagramPlugin::initFileFinish()
{
    if (m_form) {
        QMetaObject::invokeMethod(m_form, [this]() {
            if (m_form) {
                m_form->onFileReloaded();
            }
        }, Qt::QueuedConnection);
    }
}

void DltDiagramPlugin::updateFileStart()
{
}

void DltDiagramPlugin::updateMsg(int /*index*/, QDltMsg &/*msg*/)
{
}

void DltDiagramPlugin::updateMsgDecoded(int index, QDltMsg &msg)
{
    m_engine.processMessage(index, msg);
    m_lastUpdatedIndex = index;
}

void DltDiagramPlugin::updateFileFinish()
{
    if (m_form) {
        const int latest = m_lastUpdatedIndex;
        QMetaObject::invokeMethod(m_form, [this, latest]() {
            if (m_form) {
                m_form->onLiveEventsUpdated(latest);
            }
        }, Qt::QueuedConnection);
    }
}

void DltDiagramPlugin::selectedIdxMsg(int /*index*/, QDltMsg &/*msg*/)
{
}

void DltDiagramPlugin::selectedIdxMsgDecoded(int index, QDltMsg &msg)
{
    m_engine.processMessage(index, msg);

    if (m_form) {
        m_form->onSelectedLogChanged(index);
    }
}

bool DltDiagramPlugin::initControl(QDltControl *control)
{
    m_dltControl = control;
    return true;
}

bool DltDiagramPlugin::initConnections(QStringList /*list*/)
{
    return true;
}

bool DltDiagramPlugin::controlMsg(int /*index*/, QDltMsg &/*msg*/)
{
    return true;
}

bool DltDiagramPlugin::stateChanged(int /*index*/, QDltConnection::QDltConnectionState /*connectionState*/, QString /*hostname*/)
{
    return true;
}

bool DltDiagramPlugin::autoscrollStateChanged(bool /*enabled*/)
{
    return true;
}

void DltDiagramPlugin::initMessageDecoder(QDltMessageDecoder *messageDecoder)
{
    m_messageDecoder = messageDecoder;
}

void DltDiagramPlugin::initMainTableView(QTableView *pTableView)
{
    m_mainTableView = pTableView;
}

void DltDiagramPlugin::configurationChanged()
{
}

void DltDiagramPlugin::onJumpToLogRequested(int dltIndex)
{
    if (dltIndex < 0) {
        return;
    }
    if (m_dltControl) {
        m_dltControl->jumpToMsg(dltIndex);
    }
}

void DltDiagramPlugin::onApplyJsonConfigRequested(const QString &jsonText)
{
    if (applyJsonString(jsonText)) {
        rescanLoadedFile();
    }
}

void DltDiagramPlugin::rescanLoadedFile()
{
    m_engine.clearEvents();

    if (m_dltFile && m_dltFile->size() > 0) {
        const int total = m_dltFile->size();
        QDltMsg msg;
        for (int i = 0; i < total; ++i) {
            if (!m_dltFile->getMsg(i, msg)) {
                continue;
            }
            if (m_messageDecoder) {
                m_messageDecoder->decodeMsg(msg, 0);
            }
            m_engine.processMessage(i, msg);
            m_lastUpdatedIndex = i;
        }
    }

    if (m_form) {
        m_form->onFileReloaded();
    }
}
