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
 * \file dltdiagramplugin.h
 * @licence end@
 */

#ifndef DLTDIAGRAMPLUGIN_H
#define DLTDIAGRAMPLUGIN_H

#include <QObject>
#include <QPointer>
#include "plugininterface.h"
#include "diagramengine.h"
#include "form.h"

#define DLT_DIAGRAM_PLUGIN_VERSION "1.0.0"

class DltDiagramPlugin : public QObject,
                         public QDLTPluginInterface,
                         public QDltPluginViewerInterface,
                         public QDltPluginControlInterface
{
    Q_OBJECT
    Q_INTERFACES(QDLTPluginInterface)
    Q_INTERFACES(QDltPluginViewerInterface)
    Q_INTERFACES(QDltPluginControlInterface)
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID "org.genivi.DLT.DltDiagramPlugin")
#endif

public:
    DltDiagramPlugin();
    ~DltDiagramPlugin() override;

    /* QDLTPluginInterface interface */
    QString name() override;
    QString pluginVersion() override;
    QString pluginInterfaceVersion() override;
    QString description() override;
    QString error() override;
    bool loadConfig(QString filename) override;
    bool saveConfig(QString filename) override;
    QStringList infoConfig() override;

    /* QDltPluginViewerInterface */
    QWidget* initViewer() override;
    void initFileStart(QDltFile *file) override;
    void initFileFinish() override;
    void initMsg(int index, QDltMsg &msg) override;
    void initMsgDecoded(int index, QDltMsg &msg) override;
    void updateFileStart() override;
    void updateMsg(int index, QDltMsg &msg) override;
    void updateMsgDecoded(int index, QDltMsg &msg) override;
    void updateFileFinish() override;
    void selectedIdxMsg(int index, QDltMsg &msg) override;
    void selectedIdxMsgDecoded(int index, QDltMsg &msg) override;

    /* QDltPluginControlInterface */
    bool initControl(QDltControl *control) override;
    bool initConnections(QStringList list) override;
    bool controlMsg(int index, QDltMsg &msg) override;
    bool stateChanged(int index, QDltConnection::QDltConnectionState connectionState, QString hostname) override;
    bool autoscrollStateChanged(bool enabled) override;
    void initMessageDecoder(QDltMessageDecoder *messageDecoder) override;
    void initMainTableView(QTableView *pTableView) override;
    void configurationChanged() override;

private slots:
    void onJumpToLogRequested(int dltIndex);
    void onApplyJsonConfigRequested(const QString &jsonText);
    void rescanLoadedFile();

private:
    bool applyJsonString(const QString &jsonText);

    QPointer<DltDiagram::Form> m_form;
    DltDiagram::DiagramStateEngine m_engine;
    QDltFile *m_dltFile = nullptr;
    QDltControl *m_dltControl = nullptr;
    QDltMessageDecoder *m_messageDecoder = nullptr;
    QTableView *m_mainTableView = nullptr;
    QString m_rawJsonConfig;
    QString m_errorText;
    int m_lastUpdatedIndex = -1;
};

#endif // DLTDIAGRAMPLUGIN_H
