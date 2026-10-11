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
 * \file diagrammodel.h
 * @licence end@
 */

#ifndef DIAGRAMMODEL_H
#define DIAGRAMMODEL_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QMap>
#include <QPair>
#include <QRegularExpression>

namespace DltDiagram {

enum class DiagramType {
    StateDiagram = 0,
    SequenceDiagram = 1,
    FlowDiagram = 2
};

enum class DiagramFormat {
    Mermaid,
    PlantUML
};

enum class DiagramNodeShape {
    Standard = 0,  // Rounded rect (State) or Rectangle (Flow step)
    StartEnd = 1,  // Stadium / Pill shape ([Start])
    Decision = 2,  // Diamond shape {Condition?}
    Subroutine = 3 // Double-bordered process [[Subroutine]]
};

struct DiagramNode {
    QString id;
    QString label;
    bool isInitialTarget = false;
    DiagramNodeShape shape = DiagramNodeShape::Standard;
};

struct DiagramEdge {
    QString from;
    QString to;
    QString label; // Transition Name
    QString arrowStyle = QStringLiteral("sync"); // "sync", "async", "reply"
    // Optional direct DLT Log Matching on the Transition:
    QString ecuid;
    QString apid;
    QString ctid;
    QString logMatch; // Payload keyword or regex pattern
};

struct DiagramRule {
    QString name;
    QString ecuid;
    QString apid;
    QString ctid;
    QString payloadRegexString;
    QRegularExpression payloadRegex;
    QString fromTemplate;   // e.g. "$1", "*", or literal state/participant/step name
    QString toTemplate;     // e.g. "$2" or literal state/participant/step name
    QString labelTemplate;  // e.g. "$3" or literal event/message/flow label
    QString arrowStyle = QStringLiteral("sync");
};

struct DiagramDefinition {
    QString tabName = QStringLiteral("Default");
    QString id;
    QString title;
    DiagramType type = DiagramType::StateDiagram;
    DiagramFormat format = DiagramFormat::Mermaid;
    QString initialState;
    QString diagramSource; // Raw PlantUML or Mermaid script (optional)
    QList<DiagramNode> nodes;
    QList<DiagramEdge> edges;
    QList<DiagramRule> rules;

    QString favoriteKey() const { return tabName + QStringLiteral("::") + id; }
    static QString typeToString(DiagramType t);
    static DiagramType typeFromString(const QString &s);
    static QString shapeToString(DiagramNodeShape sh);
    static DiagramNodeShape shapeFromString(const QString &s);

    void ensureNode(const QString &nodeId,
                    const QString &nodeLabel = QString(),
                    bool initialTarget = false,
                    DiagramNodeShape shape = DiagramNodeShape::Standard);
    void ensureEdge(const QString &from,
                    const QString &to,
                    const QString &label = QString(),
                    const QString &arrowStyle = QStringLiteral("sync"),
                    const QString &ecuid = QString(),
                    const QString &apid = QString(),
                    const QString &ctid = QString(),
                    const QString &logMatch = QString());
};

struct DiagramTabDocument {
    QString tabName;
    QString tabColor;
    QString filePath;
    QList<DiagramDefinition> diagrams;

    void sortDiagramsByName();
};

struct DiagramEvent {
    int dltIndex = -1;
    QString timeString;
    double timestampSec = 0.0;
    QString ecuid;
    QString apid;
    QString ctid;
    QString from;
    QString to;
    QString label;
    QString arrowStyle = QStringLiteral("sync");
    QString ruleName;
    QString payload;
};

struct DiagramSnapshot {
    int queryDltIndex = -1;       // -1 means latest
    QString currentState;
    int currentStateLogIndex = -1;
    QString currentStateTime;
    int activeEventIndex = -1;    // Index inside events vector (-1 if before any event)
    bool exactMatchAtIndex = false;

    QMap<QString, int> stateVisitCounts;
    QMap<QString, int> stateLastLogIndex;
    QMap<QString, QString> stateLastTime;

    QMap<QPair<QString, QString>, int> transitionCounts;
    QMap<QPair<QString, QString>, int> transitionLastLogIndex;
    QMap<QPair<QString, QString>, QString> transitionLastLabel;
};

class DiagramParser {
public:
    static bool parseJsonConfig(const QString &jsonText,
                                QList<DiagramDefinition> &outDiagrams,
                                QString &errorOut,
                                const QString &defaultTabName = QStringLiteral("Default"),
                                QString *outTabColor = nullptr);
    static QString toJsonConfig(const QList<DiagramDefinition> &diagrams,
                                const QString &tabName = QString(),
                                const QString &tabColor = QString());

    static void parseDiagramSource(const QString &source, DiagramDefinition &def);
    static void sortDiagramsAlphabetically(QList<DiagramDefinition> &diagrams);

    static QString defaultSampleJson();
};

class DiagramExporter {
public:
    static QString generateMermaid(const DiagramDefinition &def,
                                   const QVector<DiagramEvent> &events,
                                   const DiagramSnapshot &snapshot);

    static QString generatePlantUml(const DiagramDefinition &def,
                                    const QVector<DiagramEvent> &events,
                                    const DiagramSnapshot &snapshot);
};

} // namespace DltDiagram

#endif // DIAGRAMMODEL_H
