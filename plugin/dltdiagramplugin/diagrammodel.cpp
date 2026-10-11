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
 * \file diagrammodel.cpp
 * @licence end@
 */

#include "diagrammodel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QSet>
#include <QTextStream>
#include <algorithm>

namespace DltDiagram {

QString DiagramDefinition::typeToString(DiagramType t)
{
    switch (t) {
    case DiagramType::SequenceDiagram:
        return QStringLiteral("sequence");
    case DiagramType::FlowDiagram:
        return QStringLiteral("flow");
    case DiagramType::StateDiagram:
    default:
        return QStringLiteral("state");
    }
}

DiagramType DiagramDefinition::typeFromString(const QString &s)
{
    const QString lower = s.trimmed().toLower();
    if (lower == QStringLiteral("sequence") || lower == QStringLiteral("seq")) {
        return DiagramType::SequenceDiagram;
    }
    if (lower == QStringLiteral("flow") || lower == QStringLiteral("flowchart") || lower == QStringLiteral("graph")) {
        return DiagramType::FlowDiagram;
    }
    return DiagramType::StateDiagram;
}

QString DiagramDefinition::shapeToString(DiagramNodeShape sh)
{
    switch (sh) {
    case DiagramNodeShape::StartEnd:
        return QStringLiteral("start_end");
    case DiagramNodeShape::Decision:
        return QStringLiteral("decision");
    case DiagramNodeShape::Subroutine:
        return QStringLiteral("subroutine");
    case DiagramNodeShape::Standard:
    default:
        return QStringLiteral("standard");
    }
}

DiagramNodeShape DiagramDefinition::shapeFromString(const QString &s)
{
    const QString lower = s.trimmed().toLower();
    if (lower == QStringLiteral("start_end") || lower == QStringLiteral("start") || lower == QStringLiteral("end") || lower == QStringLiteral("pill")) {
        return DiagramNodeShape::StartEnd;
    }
    if (lower == QStringLiteral("decision") || lower == QStringLiteral("diamond") || lower == QStringLiteral("condition")) {
        return DiagramNodeShape::Decision;
    }
    if (lower == QStringLiteral("subroutine") || lower == QStringLiteral("process")) {
        return DiagramNodeShape::Subroutine;
    }
    return DiagramNodeShape::Standard;
}

void DiagramDefinition::ensureNode(const QString &nodeId,
                                   const QString &nodeLabel,
                                   bool initialTarget,
                                   DiagramNodeShape shape)
{
    const QString trimmedId = nodeId.trimmed();
    if (trimmedId.isEmpty() || trimmedId == QStringLiteral("[*]") || trimmedId == QStringLiteral("*")) {
        return;
    }

    for (DiagramNode &node : nodes) {
        if (node.id == trimmedId) {
            if (!nodeLabel.trimmed().isEmpty() && (node.label.isEmpty() || node.label == node.id)) {
                node.label = nodeLabel.trimmed();
            }
            if (initialTarget) {
                node.isInitialTarget = true;
            }
            if (shape != DiagramNodeShape::Standard && node.shape == DiagramNodeShape::Standard) {
                node.shape = shape;
            }
            return;
        }
    }

    DiagramNode newNode;
    newNode.id = trimmedId;
    newNode.label = nodeLabel.trimmed().isEmpty() ? trimmedId : nodeLabel.trimmed();
    newNode.isInitialTarget = initialTarget;
    newNode.shape = shape;
    nodes.append(newNode);
}

void DiagramDefinition::ensureEdge(const QString &from,
                                   const QString &to,
                                   const QString &label,
                                   const QString &arrowStyle,
                                   const QString &ecuid,
                                   const QString &apid,
                                   const QString &ctid,
                                   const QString &logMatch)
{
    const QString f = from.trimmed();
    const QString t = to.trimmed();
    const QString l = label.trimmed();
    if (f.isEmpty() || t.isEmpty() || f == QStringLiteral("[*]") || t == QStringLiteral("[*]")) {
        return;
    }

    if (f != QStringLiteral("*")) {
        ensureNode(f);
    }
    if (t != QStringLiteral("*")) {
        ensureNode(t);
    }

    for (DiagramEdge &edge : edges) {
        if (edge.from == f && edge.to == t &&
            (edge.label == l || edge.label.isEmpty() || l.isEmpty())) {
            if (!l.isEmpty() && edge.label.isEmpty()) {
                edge.label = l;
            }
            if (!ecuid.trimmed().isEmpty()) edge.ecuid = ecuid.trimmed();
            if (!apid.trimmed().isEmpty()) edge.apid = apid.trimmed();
            if (!ctid.trimmed().isEmpty()) edge.ctid = ctid.trimmed();
            if (!logMatch.trimmed().isEmpty()) edge.logMatch = logMatch.trimmed();
            return;
        }
    }

    DiagramEdge newEdge;
    newEdge.from = f;
    newEdge.to = t;
    newEdge.label = l;
    newEdge.arrowStyle = arrowStyle.isEmpty() ? QStringLiteral("sync") : arrowStyle;
    newEdge.ecuid = ecuid.trimmed();
    newEdge.apid = apid.trimmed();
    newEdge.ctid = ctid.trimmed();
    newEdge.logMatch = logMatch.trimmed();
    edges.append(newEdge);
}

void DiagramTabDocument::sortDiagramsByName()
{
    DiagramParser::sortDiagramsAlphabetically(diagrams);
}

void DiagramParser::sortDiagramsAlphabetically(QList<DiagramDefinition> &diagrams)
{
    std::stable_sort(diagrams.begin(), diagrams.end(),
                     [](const DiagramDefinition &a, const DiagramDefinition &b) {
                         if (a.type != b.type) {
                             return static_cast<int>(a.type) < static_cast<int>(b.type);
                         }
                         const int cmp = QString::compare(a.title, b.title, Qt::CaseInsensitive);
                         if (cmp != 0) {
                             return cmp < 0;
                         }
                         return QString::compare(a.id, b.id, Qt::CaseInsensitive) < 0;
                     });
}

static QString stripQuotes(const QString &str)
{
    QString s = str.trimmed();
    if (s.size() >= 2 && ((s.startsWith('"') && s.endsWith('"')) || (s.startsWith('\'') && s.endsWith('\'')))) {
        return s.mid(1, s.size() - 2).trimmed();
    }
    return s;
}

// Helper to parse Mermaid flowchart node token like:
//   StepA([Start]) -> id="StepA", label="Start", shape=StartEnd
//   Check{Is Valid?} -> id="Check", label="Is Valid?", shape=Decision
//   Proc[Initialize] -> id="Proc", label="Initialize", shape=Standard
static void parseFlowNodeToken(const QString &token,
                               QString &outId,
                               QString &outLabel,
                               DiagramNodeShape &outShape)
{
    const QString t = token.trimmed();
    outShape = DiagramNodeShape::Standard;

    static const QRegularExpression rePill(QStringLiteral("^([\\w\\-\\.]+)\\(\\[([^\\]]+)\\]\\)$"));
    static const QRegularExpression reDiamond(QStringLiteral("^([\\w\\-\\.]+)\\{([^\\}]+)\\}$"));
    static const QRegularExpression reSub(QStringLiteral("^([\\w\\-\\.]+)\\[\\[([^\\]]+)\\]\\]$"));
    static const QRegularExpression reRect(QStringLiteral("^([\\w\\-\\.]+)\\[([^\\]]+)\\]$"));
    static const QRegularExpression reRound(QStringLiteral("^([\\w\\-\\.]+)\\(([^\\)]+)\\)$"));

    QRegularExpressionMatch m;
    if ((m = rePill.match(t)).hasMatch()) {
        outId = m.captured(1);
        outLabel = stripQuotes(m.captured(2));
        outShape = DiagramNodeShape::StartEnd;
        return;
    }
    if ((m = reDiamond.match(t)).hasMatch()) {
        outId = m.captured(1);
        outLabel = stripQuotes(m.captured(2));
        outShape = DiagramNodeShape::Decision;
        return;
    }
    if ((m = reSub.match(t)).hasMatch()) {
        outId = m.captured(1);
        outLabel = stripQuotes(m.captured(2));
        outShape = DiagramNodeShape::Subroutine;
        return;
    }
    if ((m = reRect.match(t)).hasMatch()) {
        outId = m.captured(1);
        outLabel = stripQuotes(m.captured(2));
        outShape = DiagramNodeShape::Standard;
        return;
    }
    if ((m = reRound.match(t)).hasMatch()) {
        outId = m.captured(1);
        outLabel = stripQuotes(m.captured(2));
        outShape = DiagramNodeShape::StartEnd;
        return;
    }

    outId = stripQuotes(t);
    outLabel = outId;
}

void DiagramParser::parseDiagramSource(const QString &source, DiagramDefinition &def)
{
    if (source.trimmed().isEmpty()) {
        return;
    }

    const QStringList lines = source.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);

    // Header detection
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("@startuml"), Qt::CaseInsensitive)) {
            def.format = DiagramFormat::PlantUML;
        } else if (line.startsWith(QStringLiteral("sequenceDiagram"), Qt::CaseInsensitive)) {
            def.format = DiagramFormat::Mermaid;
            def.type = DiagramType::SequenceDiagram;
        } else if (line.startsWith(QStringLiteral("stateDiagram"), Qt::CaseInsensitive)) {
            def.format = DiagramFormat::Mermaid;
            def.type = DiagramType::StateDiagram;
        } else if (line.startsWith(QStringLiteral("flowchart "), Qt::CaseInsensitive) ||
                   line.startsWith(QStringLiteral("graph "), Qt::CaseInsensitive)) {
            def.format = DiagramFormat::Mermaid;
            def.type = DiagramType::FlowDiagram;
        } else if (line.startsWith(QStringLiteral("participant "), Qt::CaseInsensitive) ||
                   line.startsWith(QStringLiteral("actor "), Qt::CaseInsensitive)) {
            if (def.edges.isEmpty() && def.nodes.isEmpty()) {
                def.type = DiagramType::SequenceDiagram;
            }
        }
    }

    static const QRegularExpression reStateAlias(
        QStringLiteral("^state\\s+\"([^\"]+)\"\\s+as\\s+([\\w\\-\\.]+)$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reStateColonLabel(
        QStringLiteral("^([\\w\\-\\.]+)\\s*:\\s*(.+)$"));
    static const QRegularExpression reParticipant1(
        QStringLiteral("^(?:participant|actor)\\s+\"([^\"]+)\"\\s+as\\s+([\\w\\-\\.]+)$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reParticipant2(
        QStringLiteral("^(?:participant|actor)\\s+([\\w\\-\\.]+)\\s+as\\s+\"?([^\"]+)\"?$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reParticipant3(
        QStringLiteral("^(?:participant|actor)\\s+\"?([^\"\\s]+)\"?$"),
        QRegularExpression::CaseInsensitiveOption);

    // Mermaid Flowchart edge with pipe label: A[...] -->|Yes| B{...}
    static const QRegularExpression reFlowPipeEdge(
        QStringLiteral("^(.+?)\\s*(?:-->|==>|-.->|->)\\s*\\|([^\\|]+)\\|\\s*(.+)$"));
    // Mermaid Flowchart edge with inline label: A[...] -- Yes --> B{...}
    static const QRegularExpression reFlowMidLabelEdge(
        QStringLiteral("^(.+?)\\s*--\\s*([^-]+?)\\s*-->\\s*(.+)$"));

    // Standard arrow line for State / Sequence / Flow
    static const QRegularExpression reArrowLine(
        QStringLiteral("^(\\[\\*\\]|\"[^\"]+\"|[^\\s\\->]+(?:\\([^\\)]*\\)|\\[[^\\]]*\\]|\\{[^\\}]*\\})?)\\s*(-->>|->>|-->|==>|-.->|->|<-|<--)\\+?\\-?\\s*(\\[\\*\\]|\"[^\"]+\"|[^:\\s]+(?:\\([^\\)]*\\)|\\[[^\\]]*\\]|\\{[^\\}]*\\})?)(?:\\s*:\\s*(.*))?$"));

    static const QRegularExpression reInlineRule(
        QStringLiteral("^(?:%%|'|//|#)\\s*@rule\\s+(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reKeyVal(
        QStringLiteral("(\\w+)\\s*=\\s*(?:\"([^\"]*)\"|(\\S+))"));

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        // Check inline @rule comment first
        QRegularExpressionMatch ruleMatch = reInlineRule.match(line);
        if (ruleMatch.hasMatch()) {
            const QString kvText = ruleMatch.captured(1);
            DiagramRule rule;
            rule.name = QStringLiteral("Inline Script Rule");
            QRegularExpressionMatchIterator it = reKeyVal.globalMatch(kvText);
            while (it.hasNext()) {
                QRegularExpressionMatch m = it.next();
                const QString key = m.captured(1).toLower();
                const QString val = m.captured(2).isNull() ? m.captured(3) : m.captured(2);
                if (key == QStringLiteral("name")) rule.name = val;
                else if (key == QStringLiteral("ecuid")) rule.ecuid = val;
                else if (key == QStringLiteral("apid")) rule.apid = val;
                else if (key == QStringLiteral("ctid")) rule.ctid = val;
                else if (key == QStringLiteral("regex") || key == QStringLiteral("payloadregex")) {
                    rule.payloadRegexString = val;
                    rule.payloadRegex = QRegularExpression(val);
                }
                else if (key == QStringLiteral("from")) rule.fromTemplate = val;
                else if (key == QStringLiteral("to")) rule.toTemplate = val;
                else if (key == QStringLiteral("event") || key == QStringLiteral("message") || key == QStringLiteral("label")) {
                    rule.labelTemplate = val;
                }
                else if (key == QStringLiteral("arrow") || key == QStringLiteral("arrowtype")) {
                    rule.arrowStyle = val;
                }
            }
            if (!rule.payloadRegexString.isEmpty() && !rule.toTemplate.isEmpty()) {
                if (rule.fromTemplate.isEmpty()) {
                    rule.fromTemplate = QStringLiteral("*");
                }
                def.rules.append(rule);
            }
            continue;
        }

        if (line.startsWith(QStringLiteral("%%")) ||
            line.startsWith(QStringLiteral("'")) ||
            line.startsWith(QStringLiteral("@startuml"), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("@enduml"), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("stateDiagram"), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("sequenceDiagram"), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("flowchart "), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("graph "), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("skinparam"), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("hide "), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("title "), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("note "), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("classDef "), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("class "), Qt::CaseInsensitive) ||
            line == QStringLiteral("{") || line == QStringLiteral("}")) {
            continue;
        }

        // Check Flowchart pipe edge: A -->|label| B
        QRegularExpressionMatch mPipe = reFlowPipeEdge.match(line);
        if (mPipe.hasMatch()) {
            QString id1, lbl1, id2, lbl2;
            DiagramNodeShape sh1, sh2;
            parseFlowNodeToken(mPipe.captured(1), id1, lbl1, sh1);
            parseFlowNodeToken(mPipe.captured(3), id2, lbl2, sh2);
            const QString edgeLbl = mPipe.captured(2).trimmed();
            def.ensureNode(id1, lbl1, def.nodes.isEmpty(), sh1);
            def.ensureNode(id2, lbl2, false, sh2);
            if (def.initialState.isEmpty()) def.initialState = id1;
            def.ensureEdge(id1, id2, edgeLbl);
            continue;
        }

        // Check Flowchart mid-label edge: A -- label --> B
        QRegularExpressionMatch mMid = reFlowMidLabelEdge.match(line);
        if (mMid.hasMatch()) {
            QString id1, lbl1, id2, lbl2;
            DiagramNodeShape sh1, sh2;
            parseFlowNodeToken(mMid.captured(1), id1, lbl1, sh1);
            parseFlowNodeToken(mMid.captured(3), id2, lbl2, sh2);
            const QString edgeLbl = mMid.captured(2).trimmed();
            def.ensureNode(id1, lbl1, def.nodes.isEmpty(), sh1);
            def.ensureNode(id2, lbl2, false, sh2);
            if (def.initialState.isEmpty()) def.initialState = id1;
            def.ensureEdge(id1, id2, edgeLbl);
            continue;
        }

        QRegularExpressionMatch mStateAlias = reStateAlias.match(line);
        if (mStateAlias.hasMatch()) {
            def.ensureNode(mStateAlias.captured(2), mStateAlias.captured(1));
            continue;
        }

        QRegularExpressionMatch mp1 = reParticipant1.match(line);
        if (mp1.hasMatch()) {
            def.ensureNode(mp1.captured(2), mp1.captured(1));
            continue;
        }
        QRegularExpressionMatch mp2 = reParticipant2.match(line);
        if (mp2.hasMatch()) {
            def.ensureNode(mp2.captured(1), mp2.captured(2));
            continue;
        }
        QRegularExpressionMatch mp3 = reParticipant3.match(line);
        if (mp3.hasMatch()) {
            def.ensureNode(mp3.captured(1), mp3.captured(1));
            continue;
        }

        QRegularExpressionMatch mArrow = reArrowLine.match(line);
        if (mArrow.hasMatch()) {
            QString rawLeft = mArrow.captured(1).trimmed();
            const QString arrow = mArrow.captured(2);
            QString rawRight = mArrow.captured(3).trimmed();
            const QString label = mArrow.captured(4).trimmed();

            if (arrow.startsWith('<')) {
                qSwap(rawLeft, rawRight);
            }

            if (rawLeft == QStringLiteral("[*]")) {
                if (rawRight != QStringLiteral("[*]")) {
                    QString rId, rLbl;
                    DiagramNodeShape rSh;
                    parseFlowNodeToken(rawRight, rId, rLbl, rSh);
                    if (def.initialState.isEmpty()) {
                        def.initialState = rId;
                    }
                    def.ensureNode(rId, rLbl, true, rSh);
                }
                continue;
            }
            if (rawRight == QStringLiteral("[*]")) {
                QString lId, lLbl;
                DiagramNodeShape lSh;
                parseFlowNodeToken(rawLeft, lId, lLbl, lSh);
                def.ensureNode(lId, lLbl, false, lSh);
                continue;
            }

            QString lId, lLbl, rId, rLbl;
            DiagramNodeShape lSh = DiagramNodeShape::Standard;
            DiagramNodeShape rSh = DiagramNodeShape::Standard;
            if (def.type == DiagramType::FlowDiagram) {
                parseFlowNodeToken(rawLeft, lId, lLbl, lSh);
                parseFlowNodeToken(rawRight, rId, rLbl, rSh);
            } else {
                lId = stripQuotes(rawLeft);
                lLbl = lId;
                rId = stripQuotes(rawRight);
                rLbl = rId;
            }

            if (def.type == DiagramType::FlowDiagram && def.initialState.isEmpty() && def.nodes.isEmpty()) {
                def.initialState = lId;
            }

            def.ensureNode(lId, lLbl, lId == def.initialState, lSh);
            def.ensureNode(rId, rLbl, false, rSh);

            QString style = QStringLiteral("sync");
            if (arrow == QStringLiteral("-->") || arrow == QStringLiteral("-->>") || arrow == QStringLiteral("<--") || arrow == QStringLiteral("-.->")) {
                style = (def.type == DiagramType::SequenceDiagram) ? QStringLiteral("reply") : QStringLiteral("sync");
            } else if (arrow == QStringLiteral("->>")) {
                style = QStringLiteral("async");
            }

            def.ensureEdge(lId, rId, label, style);
            continue;
        }

        if (def.type == DiagramType::StateDiagram) {
            QRegularExpressionMatch mColon = reStateColonLabel.match(line);
            if (mColon.hasMatch()) {
                def.ensureNode(mColon.captured(1), mColon.captured(2));
            }
        } else if (def.type == DiagramType::FlowDiagram) {
            // Standalone flow node declaration like: StepA([Start Check])
            QString nId, nLbl;
            DiagramNodeShape nSh;
            parseFlowNodeToken(line, nId, nLbl, nSh);
            if (!nId.isEmpty() && nId != line) {
                def.ensureNode(nId, nLbl, def.nodes.isEmpty(), nSh);
            }
        }
    }
}

bool DiagramParser::parseJsonConfig(const QString &jsonText,
                                    QList<DiagramDefinition> &outDiagrams,
                                    QString &errorOut,
                                    const QString &defaultTabName,
                                    QString *outTabColor)
{
    outDiagrams.clear();
    errorOut.clear();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorOut = QStringLiteral("JSON parse error at offset %1: %2")
                       .arg(parseError.offset)
                       .arg(parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        errorOut = QStringLiteral("Root JSON element must be an object.");
        return false;
    }

    const QJsonObject root = doc.object();
    const QString tabNameFromRoot = root.value(QStringLiteral("tabName")).toString(defaultTabName);
    if (outTabColor) {
        *outTabColor = root.value(QStringLiteral("tabColor")).toString().trimmed();
    }

    QJsonArray diagramsArray;
    if (root.contains(QStringLiteral("diagrams")) && root.value(QStringLiteral("diagrams")).isArray()) {
        diagramsArray = root.value(QStringLiteral("diagrams")).toArray();
    } else if (root.contains(QStringLiteral("id")) || root.contains(QStringLiteral("title")) || root.contains(QStringLiteral("rules"))) {
        diagramsArray.append(root);
    }

    int idx = 0;
    for (const QJsonValue &val : diagramsArray) {
        if (!val.isObject()) {
            continue;
        }
        ++idx;
        const QJsonObject obj = val.toObject();
        DiagramDefinition def;
        def.tabName = obj.value(QStringLiteral("tabName")).toString(tabNameFromRoot);
        def.id = obj.value(QStringLiteral("id")).toString(QStringLiteral("diagram_%1").arg(idx));
        def.title = obj.value(QStringLiteral("title")).toString(def.id);
        def.type = DiagramDefinition::typeFromString(obj.value(QStringLiteral("type")).toString(QStringLiteral("state")));

        const QString fmtStr = obj.value(QStringLiteral("diagramFormat")).toString(QStringLiteral("mermaid")).toLower();
        def.format = (fmtStr == QStringLiteral("plantuml") || fmtStr == QStringLiteral("puml"))
                         ? DiagramFormat::PlantUML
                         : DiagramFormat::Mermaid;

        def.initialState = obj.value(QStringLiteral("initialState")).toString();
        def.diagramSource = obj.value(QStringLiteral("diagramSource")).toString();

        if (!def.diagramSource.isEmpty()) {
            parseDiagramSource(def.diagramSource, def);
        }

        // Parse states / participants / steps
        const QStringList nodeArrayKeys = {
            QStringLiteral("states"),
            QStringLiteral("participants"),
            QStringLiteral("nodes"),
            QStringLiteral("steps")
        };
        for (const QString &key : nodeArrayKeys) {
            const QJsonArray arr = obj.value(key).toArray();
            for (const QJsonValue &nVal : arr) {
                if (nVal.isString()) {
                    def.ensureNode(nVal.toString());
                } else if (nVal.isObject()) {
                    const QJsonObject nObj = nVal.toObject();
                    const QString nId = nObj.value(QStringLiteral("id")).toString();
                    const QString nLabel = nObj.value(QStringLiteral("label")).toString(nId);
                    const DiagramNodeShape nShape = DiagramDefinition::shapeFromString(
                        nObj.value(QStringLiteral("shape")).toString());
                    def.ensureNode(nId, nLabel, nId == def.initialState, nShape);
                }
            }
        }

        if (!def.initialState.isEmpty()) {
            def.ensureNode(def.initialState, QString(), true);
        }

        // Parse transitions / flows (including Transition Name and DLT Log Matching)
        const QStringList edgeArrayKeys = {
            QStringLiteral("transitions"),
            QStringLiteral("edges"),
            QStringLiteral("flows")
        };
        for (const QString &key : edgeArrayKeys) {
            const QJsonArray transArr = obj.value(key).toArray();
            for (const QJsonValue &tVal : transArr) {
                if (!tVal.isObject()) continue;
                const QJsonObject tObj = tVal.toObject();
                const QString from = tObj.value(QStringLiteral("from")).toString();
                const QString to = tObj.value(QStringLiteral("to")).toString();
                const QString label = tObj.value(QStringLiteral("transitionName")).toString(
                    tObj.value(QStringLiteral("event")).toString(
                        tObj.value(QStringLiteral("label")).toString()));
                const QString style = tObj.value(QStringLiteral("arrowType")).toString(QStringLiteral("sync"));
                const QString ecuid = tObj.value(QStringLiteral("ecuid")).toString();
                const QString apid = tObj.value(QStringLiteral("apid")).toString();
                const QString ctid = tObj.value(QStringLiteral("ctid")).toString();
                const QString logMatch = tObj.value(QStringLiteral("logMatch")).toString(
                    tObj.value(QStringLiteral("dltMatch")).toString(
                        tObj.value(QStringLiteral("payloadRegex")).toString(
                            tObj.value(QStringLiteral("regex")).toString())));
                def.ensureEdge(from, to, label, style, ecuid, apid, ctid, logMatch);
            }
        }

        // Parse DLT log matching rules
        const QJsonArray rulesArr = obj.value(QStringLiteral("rules")).toArray();
        for (const QJsonValue &rVal : rulesArr) {
            if (!rVal.isObject()) continue;
            const QJsonObject rObj = rVal.toObject();
            DiagramRule rule;
            rule.name = rObj.value(QStringLiteral("name")).toString(QStringLiteral("Rule"));
            rule.ecuid = rObj.value(QStringLiteral("ecuid")).toString();
            rule.apid = rObj.value(QStringLiteral("apid")).toString();
            rule.ctid = rObj.value(QStringLiteral("ctid")).toString();
            rule.payloadRegexString = rObj.value(QStringLiteral("payloadRegex")).toString(
                rObj.value(QStringLiteral("regex")).toString());
            rule.payloadRegex = QRegularExpression(rule.payloadRegexString);
            if (!rule.payloadRegex.isValid()) {
                errorOut = QStringLiteral("Invalid regex in rule '%1': %2")
                               .arg(rule.name, rule.payloadRegex.errorString());
                return false;
            }
            rule.fromTemplate = rObj.value(QStringLiteral("from")).toString(
                rObj.value(QStringLiteral("caller")).toString(QStringLiteral("*")));
            rule.toTemplate = rObj.value(QStringLiteral("to")).toString(
                rObj.value(QStringLiteral("callee")).toString());
            rule.labelTemplate = rObj.value(QStringLiteral("transitionName")).toString(
                rObj.value(QStringLiteral("event")).toString(
                    rObj.value(QStringLiteral("message")).toString(
                        rObj.value(QStringLiteral("label")).toString())));
            rule.arrowStyle = rObj.value(QStringLiteral("arrowType")).toString(QStringLiteral("sync"));

            if (!rule.fromTemplate.startsWith('$') && rule.fromTemplate != QStringLiteral("*")) {
                def.ensureNode(rule.fromTemplate);
            }
            if (!rule.toTemplate.startsWith('$') && rule.toTemplate != QStringLiteral("*")) {
                def.ensureNode(rule.toTemplate);
            }
            if (!rule.fromTemplate.startsWith('$') &&
                !rule.toTemplate.startsWith('$') && rule.toTemplate != QStringLiteral("*")) {
                def.ensureEdge(rule.fromTemplate, rule.toTemplate,
                               rule.labelTemplate.contains('$') ? rule.name : rule.labelTemplate,
                               rule.arrowStyle,
                               rule.ecuid, rule.apid, rule.ctid, rule.payloadRegexString);
            }

            def.rules.append(rule);
        }

        // Ensure every Transition with DLT Log Matching has a corresponding executable DiagramRule
        for (const DiagramEdge &edge : def.edges) {
            if (edge.logMatch.isEmpty() && edge.ecuid.isEmpty() && edge.apid.isEmpty() && edge.ctid.isEmpty()) {
                continue;
            }
            const QString regexStr = edge.logMatch.isEmpty() ? QStringLiteral(".*") : edge.logMatch;
            bool alreadyExists = false;
            for (const DiagramRule &existing : def.rules) {
                if (existing.fromTemplate == edge.from &&
                    existing.toTemplate == edge.to &&
                    existing.payloadRegexString == regexStr &&
                    existing.ecuid == edge.ecuid &&
                    existing.apid == edge.apid &&
                    existing.ctid == edge.ctid) {
                    alreadyExists = true;
                    break;
                }
            }
            if (!alreadyExists) {
                DiagramRule trRule;
                trRule.name = edge.label.isEmpty()
                                  ? QStringLiteral("%1 -> %2").arg(edge.from, edge.to)
                                  : edge.label;
                trRule.ecuid = edge.ecuid;
                trRule.apid = edge.apid;
                trRule.ctid = edge.ctid;
                trRule.payloadRegexString = regexStr;
                trRule.payloadRegex = QRegularExpression(trRule.payloadRegexString);
                if (!trRule.payloadRegex.isValid()) {
                    errorOut = QStringLiteral("Invalid DLT Log Matching regex in transition '%1 -> %2': %3")
                                   .arg(edge.from, edge.to, trRule.payloadRegex.errorString());
                    return false;
                }
                trRule.fromTemplate = edge.from;
                trRule.toTemplate = edge.to;
                trRule.labelTemplate = edge.label.isEmpty() ? trRule.name : edge.label;
                trRule.arrowStyle = edge.arrowStyle;
                def.rules.append(trRule);
            }
        }

        outDiagrams.append(def);
    }

    sortDiagramsAlphabetically(outDiagrams);
    return true;
}

QString DiagramParser::toJsonConfig(const QList<DiagramDefinition> &diagrams,
                                    const QString &tabName,
                                    const QString &tabColor)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), QStringLiteral("1.0"));
    if (!tabName.isEmpty()) {
        root.insert(QStringLiteral("tabName"), tabName);
    }
    if (!tabColor.trimmed().isEmpty()) {
        root.insert(QStringLiteral("tabColor"), tabColor.trimmed());
    }

    QList<DiagramDefinition> sorted = diagrams;
    sortDiagramsAlphabetically(sorted);

    QJsonArray diagArray;
    for (const DiagramDefinition &def : sorted) {
        QJsonObject dObj;
        dObj.insert(QStringLiteral("id"), def.id);
        dObj.insert(QStringLiteral("title"), def.title);
        dObj.insert(QStringLiteral("type"), DiagramDefinition::typeToString(def.type));
        dObj.insert(QStringLiteral("diagramFormat"),
                    def.format == DiagramFormat::PlantUML ? QStringLiteral("plantuml") : QStringLiteral("mermaid"));
        if (!def.initialState.isEmpty()) {
            dObj.insert(QStringLiteral("initialState"), def.initialState);
        }
        if (!def.diagramSource.isEmpty()) {
            dObj.insert(QStringLiteral("diagramSource"), def.diagramSource);
        }

        QJsonArray nodesArr;
        for (const DiagramNode &node : def.nodes) {
            if (node.label == node.id && node.shape == DiagramNodeShape::Standard) {
                nodesArr.append(node.id);
            } else {
                QJsonObject nObj;
                nObj.insert(QStringLiteral("id"), node.id);
                nObj.insert(QStringLiteral("label"), node.label);
                if (node.shape != DiagramNodeShape::Standard) {
                    nObj.insert(QStringLiteral("shape"), DiagramDefinition::shapeToString(node.shape));
                }
                nodesArr.append(nObj);
            }
        }
        if (def.type == DiagramType::SequenceDiagram) {
            dObj.insert(QStringLiteral("participants"), nodesArr);
        } else if (def.type == DiagramType::FlowDiagram) {
            dObj.insert(QStringLiteral("steps"), nodesArr);
        } else {
            dObj.insert(QStringLiteral("states"), nodesArr);
        }

        if (!def.edges.isEmpty()) {
            QJsonArray edgesArr;
            for (const DiagramEdge &edge : def.edges) {
                QJsonObject eObj;
                eObj.insert(QStringLiteral("from"), edge.from);
                eObj.insert(QStringLiteral("to"), edge.to);
                if (!edge.label.isEmpty()) {
                    eObj.insert(QStringLiteral("transitionName"), edge.label);
                    eObj.insert(QStringLiteral("label"), edge.label);
                }
                if (edge.arrowStyle != QStringLiteral("sync")) {
                    eObj.insert(QStringLiteral("arrowType"), edge.arrowStyle);
                }
                if (!edge.ecuid.isEmpty()) eObj.insert(QStringLiteral("ecuid"), edge.ecuid);
                if (!edge.apid.isEmpty()) eObj.insert(QStringLiteral("apid"), edge.apid);
                if (!edge.ctid.isEmpty()) eObj.insert(QStringLiteral("ctid"), edge.ctid);
                if (!edge.logMatch.isEmpty()) eObj.insert(QStringLiteral("logMatch"), edge.logMatch);
                edgesArr.append(eObj);
            }
            dObj.insert(QStringLiteral("transitions"), edgesArr);
        }

        QJsonArray rulesArr;
        for (const DiagramRule &rule : def.rules) {
            QJsonObject rObj;
            rObj.insert(QStringLiteral("name"), rule.name);
            if (!rule.ecuid.isEmpty()) rObj.insert(QStringLiteral("ecuid"), rule.ecuid);
            if (!rule.apid.isEmpty()) rObj.insert(QStringLiteral("apid"), rule.apid);
            if (!rule.ctid.isEmpty()) rObj.insert(QStringLiteral("ctid"), rule.ctid);
            rObj.insert(QStringLiteral("payloadRegex"), rule.payloadRegexString);
            rObj.insert(QStringLiteral("from"), rule.fromTemplate);
            rObj.insert(QStringLiteral("to"), rule.toTemplate);
            if (def.type == DiagramType::SequenceDiagram) {
                rObj.insert(QStringLiteral("message"), rule.labelTemplate);
                rObj.insert(QStringLiteral("arrowType"), rule.arrowStyle);
            } else {
                rObj.insert(QStringLiteral("event"), rule.labelTemplate);
            }
            rulesArr.append(rObj);
        }
        dObj.insert(QStringLiteral("rules"), rulesArr);
        diagArray.append(dObj);
    }

    root.insert(QStringLiteral("diagrams"), diagArray);
    QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString DiagramExporter::generateMermaid(const DiagramDefinition &def,
                                         const QVector<DiagramEvent> &events,
                                         const DiagramSnapshot &snapshot)
{
    QString out;
    QTextStream s(&out);

    if (def.type == DiagramType::StateDiagram) {
        s << "stateDiagram-v2\n";
        s << "    %% Diagram: " << def.title << "\n";

        if (!def.initialState.isEmpty()) {
            s << "    [*] --> " << def.initialState << "\n";
        }

        QSet<QString> declaredNodes;
        for (const DiagramNode &node : def.nodes) {
            declaredNodes.insert(node.id);
            if (!node.label.isEmpty() && node.label != node.id) {
                s << "    state \"" << node.label << "\" as " << node.id << "\n";
            }
        }
        for (auto it = snapshot.stateVisitCounts.begin(); it != snapshot.stateVisitCounts.end(); ++it) {
            if (!declaredNodes.contains(it.key()) && !it.key().isEmpty()) {
                declaredNodes.insert(it.key());
                s << "    state " << it.key() << "\n";
            }
        }

        QSet<QPair<QString, QString>> printedEdges;
        for (const DiagramEdge &edge : def.edges) {
            const QPair<QString, QString> pair(edge.from, edge.to);
            printedEdges.insert(pair);
            QString lbl = edge.label;
            const int count = snapshot.transitionCounts.value(pair, 0);
            if (count > 0) {
                lbl = lbl.isEmpty() ? QStringLiteral("(x%1)").arg(count)
                                    : QStringLiteral("%1 (x%2)").arg(lbl).arg(count);
            }
            if (lbl.isEmpty()) {
                s << "    " << edge.from << " --> " << edge.to << "\n";
            } else {
                s << "    " << edge.from << " --> " << edge.to << " : " << lbl << "\n";
            }
        }

        for (auto it = snapshot.transitionCounts.begin(); it != snapshot.transitionCounts.end(); ++it) {
            if (printedEdges.contains(it.key())) continue;
            const QString from = it.key().first;
            const QString to = it.key().second;
            if (from.isEmpty() || to.isEmpty()) continue;
            const QString lastLbl = snapshot.transitionLastLabel.value(it.key());
            const QString lbl = lastLbl.isEmpty()
                                    ? QStringLiteral("(x%1)").arg(it.value())
                                    : QStringLiteral("%1 (x%2)").arg(lastLbl).arg(it.value());
            s << "    " << from << " --> " << to << " : " << lbl << "\n";
        }

        if (!snapshot.currentState.isEmpty()) {
            s << "\n    %% Current State Highlight\n";
            s << "    classDef activeState fill:#10b981,stroke:#047857,stroke-width:3px,color:#ffffff,font-weight:bold\n";
            s << "    class " << snapshot.currentState << " activeState\n";
            if (snapshot.currentStateLogIndex >= 0) {
                s << "    note right of " << snapshot.currentState
                  << " : CURRENT STATE (Log #" << snapshot.currentStateLogIndex
                  << " @ " << snapshot.currentStateTime << ")\n";
            }
        }
    } else if (def.type == DiagramType::FlowDiagram) {
        s << "flowchart TD\n";
        s << "    %% Flow Diagram: " << def.title << "\n";

        QSet<QString> declaredNodes;
        for (const DiagramNode &node : def.nodes) {
            declaredNodes.insert(node.id);
            const QString lbl = node.label.isEmpty() ? node.id : node.label;
            switch (node.shape) {
            case DiagramNodeShape::StartEnd:
                s << "    " << node.id << "([\"" << lbl << "\"])\n";
                break;
            case DiagramNodeShape::Decision:
                s << "    " << node.id << "{\"" << lbl << "\"}\n";
                break;
            case DiagramNodeShape::Subroutine:
                s << "    " << node.id << "[[\"" << lbl << "\"]]\n";
                break;
            case DiagramNodeShape::Standard:
            default:
                s << "    " << node.id << "[\"" << lbl << "\"]\n";
                break;
            }
        }

        QSet<QPair<QString, QString>> printedEdges;
        for (const DiagramEdge &edge : def.edges) {
            const QPair<QString, QString> pair(edge.from, edge.to);
            printedEdges.insert(pair);
            QString lbl = edge.label;
            const int count = snapshot.transitionCounts.value(pair, 0);
            if (count > 0) {
                lbl = lbl.isEmpty() ? QStringLiteral("x%1").arg(count)
                                    : QStringLiteral("%1 (x%2)").arg(lbl).arg(count);
            }
            if (lbl.isEmpty()) {
                s << "    " << edge.from << " --> " << edge.to << "\n";
            } else {
                s << "    " << edge.from << " -->|" << lbl << "| " << edge.to << "\n";
            }
        }

        for (auto it = snapshot.transitionCounts.begin(); it != snapshot.transitionCounts.end(); ++it) {
            if (printedEdges.contains(it.key())) continue;
            const QString from = it.key().first;
            const QString to = it.key().second;
            if (from.isEmpty() || to.isEmpty()) continue;
            const QString lastLbl = snapshot.transitionLastLabel.value(it.key());
            const QString lbl = lastLbl.isEmpty()
                                    ? QStringLiteral("x%1").arg(it.value())
                                    : QStringLiteral("%1 (x%2)").arg(lastLbl).arg(it.value());
            s << "    " << from << " -->|" << lbl << "| " << to << "\n";
        }

        s << "\n    classDef activeStep fill:#10b981,stroke:#047857,stroke-width:3px,color:#ffffff,font-weight:bold\n";
        s << "    classDef visitedStep fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a8a\n";
        for (auto it = snapshot.stateVisitCounts.begin(); it != snapshot.stateVisitCounts.end(); ++it) {
            if (it.value() > 0 && it.key() != snapshot.currentState) {
                s << "    class " << it.key() << " visitedStep\n";
            }
        }
        if (!snapshot.currentState.isEmpty()) {
            s << "    class " << snapshot.currentState << " activeStep\n";
        }
    } else {
        s << "sequenceDiagram\n";
        s << "    %% Diagram: " << def.title << "\n";
        for (const DiagramNode &part : def.nodes) {
            if (part.label != part.id && !part.label.isEmpty()) {
                s << "    participant " << part.id << " as \"" << part.label << "\"\n";
            } else {
                s << "    participant " << part.id << "\n";
            }
        }

        if (events.isEmpty()) {
            for (const DiagramEdge &edge : def.edges) {
                const QString arrow = (edge.arrowStyle == QStringLiteral("reply")) ? QStringLiteral("-->>") : QStringLiteral("->>");
                s << "    " << edge.from << arrow << edge.to << ": " << edge.label << "\n";
            }
        } else {
            const int maxIdx = (snapshot.activeEventIndex >= 0) ? snapshot.activeEventIndex : (events.size() - 1);
            const int startIdx = qMax(0, maxIdx - 30);
            for (int i = startIdx; i <= maxIdx && i < events.size(); ++i) {
                const DiagramEvent &ev = events[i];
                const QString arrow = (ev.arrowStyle == QStringLiteral("reply")) ? QStringLiteral("-->>") : QStringLiteral("->>");
                if (i == snapshot.activeEventIndex) {
                    s << "    rect rgb(16, 185, 129, 0.25)\n";
                    s << "    Note over " << ev.from << "," << ev.to
                      << ": ACTIVE @ Log #" << ev.dltIndex << " (" << ev.timeString << ")\n";
                    s << "    " << ev.from << arrow << ev.to << ": [#" << ev.dltIndex << "] " << ev.label << "\n";
                    s << "    end\n";
                } else {
                    s << "    " << ev.from << arrow << ev.to << ": [#" << ev.dltIndex << "] " << ev.label << "\n";
                }
            }
        }
    }

    return out;
}

QString DiagramExporter::generatePlantUml(const DiagramDefinition &def,
                                          const QVector<DiagramEvent> &events,
                                          const DiagramSnapshot &snapshot)
{
    QString out;
    QTextStream s(&out);

    s << "@startuml\n";
    s << "title " << def.title << "\n";

    if (def.type == DiagramType::StateDiagram || def.type == DiagramType::FlowDiagram) {
        s << "skinparam state {\n";
        s << "  BackgroundColor #f8fafc\n";
        s << "  BorderColor #475569\n";
        s << "  BackgroundColor<<Current>> #10b981\n";
        s << "  FontColor<<Current>> #ffffff\n";
        s << "  BorderColor<<Current>> #047857\n";
        s << "  BackgroundColor<<Visited>> #dbeafe\n";
        s << "  BorderColor<<Visited>> #2563eb\n";
        s << "}\n\n";

        if (!def.initialState.isEmpty()) {
            s << "[*] --> " << def.initialState << "\n";
        }

        QSet<QString> allStates;
        for (const DiagramNode &node : def.nodes) {
            allStates.insert(node.id);
            QString stereo;
            if (node.id == snapshot.currentState) {
                stereo = QStringLiteral(" <<Current>>");
            } else if (snapshot.stateVisitCounts.value(node.id, 0) > 0) {
                stereo = QStringLiteral(" <<Visited>>");
            }
            s << "state \"" << node.label << "\" as " << node.id << stereo << "\n";
        }
        for (auto it = snapshot.stateVisitCounts.begin(); it != snapshot.stateVisitCounts.end(); ++it) {
            if (!allStates.contains(it.key()) && !it.key().isEmpty()) {
                allStates.insert(it.key());
                const QString stereo = (it.key() == snapshot.currentState)
                                           ? QStringLiteral(" <<Current>>")
                                           : QStringLiteral(" <<Visited>>");
                s << "state \"" << it.key() << "\" as " << it.key() << stereo << "\n";
            }
        }

        QSet<QPair<QString, QString>> printedEdges;
        for (const DiagramEdge &edge : def.edges) {
            const QPair<QString, QString> pair(edge.from, edge.to);
            printedEdges.insert(pair);
            QString lbl = edge.label;
            const int count = snapshot.transitionCounts.value(pair, 0);
            if (count > 0) {
                lbl = lbl.isEmpty() ? QStringLiteral("(x%1)").arg(count)
                                    : QStringLiteral("%1 (x%2)").arg(lbl).arg(count);
            }
            if (lbl.isEmpty()) {
                s << edge.from << " --> " << edge.to << "\n";
            } else {
                s << edge.from << " --> " << edge.to << " : " << lbl << "\n";
            }
        }

        for (auto it = snapshot.transitionCounts.begin(); it != snapshot.transitionCounts.end(); ++it) {
            if (printedEdges.contains(it.key())) continue;
            const QString from = it.key().first;
            const QString to = it.key().second;
            if (from.isEmpty() || to.isEmpty()) continue;
            const QString lastLbl = snapshot.transitionLastLabel.value(it.key());
            const QString lbl = lastLbl.isEmpty()
                                    ? QStringLiteral("(x%1)").arg(it.value())
                                    : QStringLiteral("%1 (x%2)").arg(lastLbl).arg(it.value());
            s << from << " --> " << to << " : " << lbl << "\n";
        }

        if (!snapshot.currentState.isEmpty() && snapshot.currentStateLogIndex >= 0) {
            s << "note right of " << snapshot.currentState
              << " : CURRENT\\nLog #" << snapshot.currentStateLogIndex
              << " (" << snapshot.currentStateTime << ")\n";
        }
    } else {
        for (const DiagramNode &part : def.nodes) {
            s << "participant \"" << part.label << "\" as " << part.id << "\n";
        }
        s << "\n";
        if (events.isEmpty()) {
            for (const DiagramEdge &edge : def.edges) {
                const QString arrow = (edge.arrowStyle == QStringLiteral("reply")) ? QStringLiteral("-->") : QStringLiteral("->");
                s << edge.from << " " << arrow << " " << edge.to << " : " << edge.label << "\n";
            }
        } else {
            const int maxIdx = (snapshot.activeEventIndex >= 0) ? snapshot.activeEventIndex : (events.size() - 1);
            const int startIdx = qMax(0, maxIdx - 30);
            for (int i = startIdx; i <= maxIdx && i < events.size(); ++i) {
                const DiagramEvent &ev = events[i];
                const QString arrow = (ev.arrowStyle == QStringLiteral("reply"))
                                          ? QStringLiteral("-->")
                                          : ((i == snapshot.activeEventIndex) ? QStringLiteral("-[#10b981,bold]>") : QStringLiteral("->"));
                s << ev.from << " " << arrow << " " << ev.to
                  << " : [#" << ev.dltIndex << "] " << ev.label << "\n";
                if (i == snapshot.activeEventIndex) {
                    s << "note right : ACTIVE @ " << ev.timeString << "\n";
                }
            }
        }
    }

    s << "@enduml\n";
    return out;
}

QString DiagramParser::defaultSampleJson()
{
    return QStringLiteral(R"JSON({
  "version": "1.0",
  "tabName": "Default",
  "diagrams": [
    {
      "id": "ecu_lifecycle_state",
      "title": "ECU Lifecycle & Power State Machine",
      "type": "state",
      "diagramFormat": "mermaid",
      "initialState": "Init",
      "diagramSource": "stateDiagram-v2\n    [*] --> Init\n    Init --> Startup : BootComplete\n    Startup --> Run : ServicesReady\n    Run --> Degraded : FaultDetected\n    Degraded --> Run : FaultCleared\n    Run --> Shutdown : IgnitionOff\n    Shutdown --> Sleep : PowerDown\n    Sleep --> Startup : Wakeup",
      "rules": [
        {
          "name": "Explicit Transition Pattern (A -> B)",
          "apid": "",
          "ctid": "",
          "payloadRegex": "(?:State|STATE|Transition|Mode)\\s*(?:changed|transition)?\\s*[:=]?\\s*([A-Za-z0-9_]+)\\s*(?:->|=>|to)\\s*([A-Za-z0-9_]+)(?:\\s*\\(([^)]+)\\))?",
          "from": "$1",
          "to": "$2",
          "event": "$3"
        },
        {
          "name": "Enter State Pattern",
          "apid": "",
          "ctid": "",
          "payloadRegex": "(?:Enter(?:ing)?|Current)\\s+(?:state|mode)\\s*[:=]\\s*([A-Za-z0-9_]+)",
          "from": "*",
          "to": "$1",
          "event": "Enter $1"
        },
        {
          "name": "DLT Connection / Control State",
          "apid": "DLT",
          "ctid": "",
          "payloadRegex": "(connected|disconnected|timeout)",
          "from": "*",
          "to": "$1",
          "event": "DLT $1"
        }
      ]
    },
    {
      "id": "service_ipc_sequence",
      "title": "Component / APID Message Sequence",
      "type": "sequence",
      "diagramFormat": "plantuml",
      "diagramSource": "@startuml\nparticipant HMI\nparticipant MediaMgr\nparticipant AudioDaemon\nparticipant SystemMgr\n@enduml",
      "rules": [
        {
          "name": "Explicit Arrow Log ([Caller -> Callee] Msg)",
          "apid": "",
          "ctid": "",
          "payloadRegex": "\\[?([A-Za-z0-9_]+)\\s*(->|-->)\\s*([A-Za-z0-9_]+)\\]?\\s*:\\s*(.+)",
          "from": "$1",
          "to": "$3",
          "message": "$4",
          "arrowType": "sync"
        },
        {
          "name": "Send / Request Pattern",
          "apid": "",
          "ctid": "",
          "payloadRegex": "(?:send|request|call)\\s+to\\s+([A-Za-z0-9_]+)\\s*:\\s*(.+)",
          "from": "$APID",
          "to": "$1",
          "message": "$2",
          "arrowType": "sync"
        },
        {
          "name": "APID -> CTID Activity Sequence",
          "apid": "",
          "ctid": "",
          "payloadRegex": "^(.{1,80})",
          "from": "$ECUID_$APID",
          "to": "$APID_$CTID",
          "message": "$1",
          "arrowType": "sync"
        }
      ]
    },
    {
      "id": "ota_update_flow",
      "title": "OTA Package Verification & Install Flow",
      "type": "flow",
      "diagramFormat": "mermaid",
      "initialState": "DownloadPkg",
      "diagramSource": "flowchart TD\n    DownloadPkg([Download Package]) --> VerifySig{Signature Valid?}\n    VerifySig -->|Yes| CheckSpace{Storage OK?}\n    VerifySig -->|No| AbortUpdate([Abort & Rollback])\n    CheckSpace -->|Yes| ApplyDelta[Install Delta Image]\n    CheckSpace -->|No| AbortUpdate\n    ApplyDelta --> RebootEcu([Activate & Reboot])",
      "rules": [
        {
          "name": "Flow Step Log",
          "apid": "",
          "ctid": "",
          "payloadRegex": "(?:Flow|Step|Stage)\\s*[:=]\\s*([A-Za-z0-9_]+)(?:\\s*->\\s*([A-Za-z0-9_]+))?(?:\\s*\\(([^)]+)\\))?",
          "from": "$1",
          "to": "$2",
          "event": "$3"
        },
        {
          "name": "Enter Flow Step",
          "apid": "",
          "ctid": "",
          "payloadRegex": "Step\\s+enter\\s*[:=]\\s*([A-Za-z0-9_]+)",
          "from": "*",
          "to": "$1",
          "event": "Step $1"
        }
      ]
    }
  ]
}
)JSON");
}

} // namespace DltDiagram
