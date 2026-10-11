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
 * \file diagramgraphicsview.cpp
 * @licence end@
 */

#include "diagramgraphicsview.h"

#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsPolygonItem>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIcon>
#include <QPen>
#include <QBrush>
#include <QLinearGradient>
#include <QFont>
#include <QQueue>
#include <QSet>
#include <QtMath>

namespace DltDiagram {

namespace {
constexpr int kDataKeyLogIndex = 100;
constexpr int kDataKeyStateId = 101;
constexpr qreal kStateNodeWidth = 172.0;
constexpr qreal kStateNodeHeight = 74.0;

QIcon createMagnifierIcon(bool zoomIn)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setPen(QPen(QColor(30, 41, 59), 1.8));
    p.setBrush(QColor(241, 245, 249));
    p.drawEllipse(QRectF(2.0, 2.0, 9.0, 9.0));
    p.drawLine(QPointF(9.8, 9.8), QPointF(14.0, 14.0));

    p.setPen(QPen(QColor(15, 23, 42), 1.6));
    p.drawLine(QPointF(4.5, 6.5), QPointF(8.5, 6.5));
    if (zoomIn) {
        p.drawLine(QPointF(6.5, 4.5), QPointF(6.5, 8.5));
    }
    return QIcon(pix);
}
} // namespace

DiagramGraphicsView::DiagramGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setBackgroundBrush(QColor(248, 250, 252));

    setupOverlayControls();
}

void DiagramGraphicsView::setupOverlayControls()
{
    m_overlayWidget = new QWidget(viewport());
    m_overlayWidget->setStyleSheet(QStringLiteral(
        "QWidget { background-color: rgba(255, 255, 255, 225); border: 1px solid #cbd5e1; border-radius: 6px; }"));

    QHBoxLayout *lay = new QHBoxLayout(m_overlayWidget);
    lay->setContentsMargins(4, 3, 4, 3);
    lay->setSpacing(4);

    m_btnZoomIn = new QPushButton(m_overlayWidget);
    m_btnZoomIn->setIcon(createMagnifierIcon(true));
    m_btnZoomIn->setFixedSize(26, 24);
    m_btnZoomIn->setToolTip(QStringLiteral("Zoom In (확대)"));
    m_btnZoomIn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 4px; }"
        "QPushButton:hover { background-color: #e2e8f0; }"));
    connect(m_btnZoomIn, &QPushButton::clicked, this, [this]() {
        setAutoFitEnabled(false);
        zoomInView();
    });

    m_btnZoomOut = new QPushButton(m_overlayWidget);
    m_btnZoomOut->setIcon(createMagnifierIcon(false));
    m_btnZoomOut->setFixedSize(26, 24);
    m_btnZoomOut->setToolTip(QStringLiteral("Zoom Out (축소)"));
    m_btnZoomOut->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 4px; }"
        "QPushButton:hover { background-color: #e2e8f0; }"));
    connect(m_btnZoomOut, &QPushButton::clicked, this, [this]() {
        setAutoFitEnabled(false);
        zoomOutView();
    });

    m_btnAutoFit = new QPushButton(QStringLiteral("AutoFit"), m_overlayWidget);
    m_btnAutoFit->setCheckable(true);
    m_btnAutoFit->setChecked(m_autoFitEnabled);
    m_btnAutoFit->setFixedHeight(24);
    m_btnAutoFit->setToolTip(QStringLiteral("AutoFit 활성화: 창 크기 및 다이어그램 변경 시 화면에 자동으로 맞춤"));
    connect(m_btnAutoFit, &QPushButton::toggled, this, &DiagramGraphicsView::setAutoFitEnabled);

    lay->addWidget(m_btnZoomIn);
    lay->addWidget(m_btnZoomOut);
    lay->addWidget(m_btnAutoFit);
    m_overlayWidget->adjustSize();

    updateAutoFitButtonStyle();
    repositionOverlayControls();
}

void DiagramGraphicsView::updateAutoFitButtonStyle()
{
    if (!m_btnAutoFit) return;
    m_btnAutoFit->blockSignals(true);
    m_btnAutoFit->setChecked(m_autoFitEnabled);
    m_btnAutoFit->blockSignals(false);

    if (m_autoFitEnabled) {
        m_btnAutoFit->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #2563eb; color: #ffffff; font-weight: bold; "
            "border: 1px solid #1d4ed8; border-radius: 4px; padding: 0px 8px; }"));
    } else {
        m_btnAutoFit->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #f8fafc; color: #334155; "
            "border: 1px solid #cbd5e1; border-radius: 4px; padding: 0px 8px; }"
            "QPushButton:hover { background-color: #e2e8f0; }"));
    }
}

void DiagramGraphicsView::repositionOverlayControls()
{
    if (!m_overlayWidget || !viewport()) return;
    m_overlayWidget->adjustSize();
    const int x = qMax(8, viewport()->width() - m_overlayWidget->width() - 10);
    const int y = 8;
    m_overlayWidget->move(x, y);
    m_overlayWidget->raise();
}

void DiagramGraphicsView::setAutoFitEnabled(bool enabled)
{
    m_autoFitEnabled = enabled;
    updateAutoFitButtonStyle();
    if (m_autoFitEnabled) {
        fitDiagramInView();
    }
}

void DiagramGraphicsView::resetCustomLayout()
{
    m_customNodePositions.clear();
}

void DiagramGraphicsView::zoomInView()
{
    scale(1.2, 1.2);
}

void DiagramGraphicsView::zoomOutView()
{
    scale(1.0 / 1.2, 1.0 / 1.2);
}

void DiagramGraphicsView::fitDiagramInView()
{
    if (!m_scene || m_scene->items().isEmpty()) {
        return;
    }
    QRectF bounds = m_scene->itemsBoundingRect().adjusted(-40, -40, 40, 40);
    fitInView(bounds, Qt::KeepAspectRatio);
}

void DiagramGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    repositionOverlayControls();
    if (m_autoFitEnabled) {
        fitDiagramInView();
    }
}

void DiagramGraphicsView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    repositionOverlayControls();
    if (m_autoFitEnabled) {
        fitDiagramInView();
    }
}

void DiagramGraphicsView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier || m_scene->height() < 900) {
        setAutoFitEnabled(false);
        const double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        scale(factor, factor);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void DiagramGraphicsView::mousePressEvent(QMouseEvent *event)
{
    m_pressPos = event->pos();
    QGraphicsView::mousePressEvent(event);
}

void DiagramGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);

    const int moveDist = (event->pos() - m_pressPos).manhattanLength();
    if (moveDist < 6 && event->button() == Qt::LeftButton) {
        const QList<QGraphicsItem *> clickedItems = items(event->pos());
        for (QGraphicsItem *item : clickedItems) {
            QVariant logIdxVar = item->data(kDataKeyLogIndex);
            if (!logIdxVar.isValid() && item->parentItem()) {
                logIdxVar = item->parentItem()->data(kDataKeyLogIndex);
            }
            if (logIdxVar.isValid()) {
                const int dltIdx = logIdxVar.toInt();
                if (dltIdx >= 0) {
                    emit jumpToLogRequested(dltIdx);
                    break;
                }
            }
        }
    }
}

void DiagramGraphicsView::clearCanvasWithMessage(const QString &message)
{
    m_currentDiagramKey.clear();
    m_customNodePositions.clear();
    resetTransform();
    m_scene->clear();

    QGraphicsSimpleTextItem *info = m_scene->addSimpleText(message);
    QFont f = info->font();
    f.setPointSize(10);
    f.setBold(true);
    info->setFont(f);
    info->setBrush(QColor(100, 116, 139));
    info->setPos(20.0, 20.0);

    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-40, -40, 40, 40));
    repositionOverlayControls();
}

void DiagramGraphicsView::renderDiagram(const DiagramDefinition &def,
                                        const QVector<DiagramEvent> &events,
                                        const DiagramSnapshot &snapshot)
{
    const QString key = def.favoriteKey();
    if (m_currentDiagramKey != key) {
        m_currentDiagramKey = key;
        m_customNodePositions.clear();
        resetTransform();
    }

    m_scene->clear();

    if (def.type == DiagramType::SequenceDiagram) {
        renderSequenceDiagram(def, events, snapshot);
    } else if (def.type == DiagramType::FlowDiagram) {
        renderStateOrFlowDiagram(def, events, snapshot, true);
    } else {
        renderStateOrFlowDiagram(def, events, snapshot, false);
    }

    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-55, -55, 55, 55));
    repositionOverlayControls();
    if (m_autoFitEnabled) {
        fitDiagramInView();
    }
}

void DiagramGraphicsView::addArrowHead(QGraphicsScene *scene,
                                       const QPointF &tip,
                                       const QPointF &fromDir,
                                       const QColor &color,
                                       int logIndex,
                                       const QString &tooltip)
{
    const QPointF diff = tip - fromDir;
    const double len = std::hypot(diff.x(), diff.y());
    if (len < 1e-3) {
        return;
    }

    const QPointF unit = diff / len;
    const QPointF perp(-unit.y(), unit.x());
    const double arrowLen = 10.0;
    const double arrowWidth = 5.5;

    QPolygonF head;
    head << tip
         << (tip - unit * arrowLen + perp * arrowWidth)
         << (tip - unit * arrowLen - perp * arrowWidth);

    QGraphicsPolygonItem *poly = scene->addPolygon(head, QPen(color, 1.5), QBrush(color));
    poly->setZValue(3.0);
    if (logIndex >= 0) {
        poly->setData(kDataKeyLogIndex, logIndex);
        poly->setCursor(Qt::PointingHandCursor);
    }
    if (!tooltip.isEmpty()) {
        poly->setToolTip(tooltip);
    }
}

QMap<QString, QPointF> DiagramGraphicsView::computeLayeredLayout(
    const DiagramDefinition &def,
    const QStringList &stateIds,
    const QList<QPair<QString, QString>> &allEdges,
    bool topDownFlow) const
{
    QMap<QString, QPointF> positions;
    if (stateIds.isEmpty()) {
        return positions;
    }

    QMap<QString, int> layerOf;
    QQueue<QString> queue;

    QString rootState = def.initialState;
    if (rootState.isEmpty() || !stateIds.contains(rootState)) {
        rootState = stateIds.first();
    }

    layerOf[rootState] = 0;
    queue.enqueue(rootState);

    while (!queue.isEmpty()) {
        const QString curr = queue.dequeue();
        const int currLayer = layerOf.value(curr, 0);
        for (const auto &edge : allEdges) {
            if (edge.first == curr && !layerOf.contains(edge.second)) {
                layerOf[edge.second] = currLayer + 1;
                queue.enqueue(edge.second);
            }
        }
    }

    int maxLayer = 0;
    for (int l : layerOf.values()) {
        if (l > maxLayer) maxLayer = l;
    }
    for (const QString &st : stateIds) {
        if (!layerOf.contains(st)) {
            layerOf[st] = ++maxLayer;
        }
    }

    QMap<int, QStringList> layers;
    for (const QString &st : stateIds) {
        layers[layerOf.value(st, 0)].append(st);
    }

    const qreal colSpacing = topDownFlow ? 240.0 : 265.0;
    const qreal rowSpacing = topDownFlow ? 135.0 : 150.0;

    int layerIdx = 0;
    for (auto it = layers.begin(); it != layers.end(); ++it, ++layerIdx) {
        const QStringList &nodesInLayer = it.value();
        const int count = nodesInLayer.size();
        for (int r = 0; r < count; ++r) {
            const QString &stateId = nodesInLayer[r];
            if (m_customNodePositions.contains(stateId)) {
                positions[stateId] = m_customNodePositions.value(stateId);
            } else if (topDownFlow) {
                // Top-Down layout for Flow Diagrams
                const qreal xOffset = (r - (count - 1) / 2.0) * colSpacing;
                const qreal x = 360.0 + xOffset;
                const qreal y = 90.0 + layerIdx * rowSpacing;
                positions[stateId] = QPointF(x, y);
            } else {
                // Left-to-Right layout for State Diagrams
                const qreal x = 130.0 + layerIdx * colSpacing;
                const qreal yOffset = (r - (count - 1) / 2.0) * rowSpacing;
                const qreal y = 220.0 + yOffset;
                positions[stateId] = QPointF(x, y);
            }
        }
    }

    return positions;
}

static QPointF intersectNodeRect(const QPointF &center, const QPointF &target, qreal halfW, qreal halfH)
{
    const QPointF d = target - center;
    if (qAbs(d.x()) < 1e-4 && qAbs(d.y()) < 1e-4) {
        return center;
    }
    const qreal tx = (qAbs(d.x()) > 1e-4) ? (halfW / qAbs(d.x())) : 1e9;
    const qreal ty = (qAbs(d.y()) > 1e-4) ? (halfH / qAbs(d.y())) : 1e9;
    const qreal t = qMin(tx, ty);
    return center + d * t;
}

void DiagramGraphicsView::renderStateOrFlowDiagram(const DiagramDefinition &def,
                                                   const QVector<DiagramEvent> &events,
                                                   const DiagramSnapshot &snapshot,
                                                   bool isFlowDiagram)
{
    QStringList stateIds;
    QMap<QString, QString> stateLabels;
    QMap<QString, DiagramNodeShape> stateShapes;

    auto registerState = [&](const QString &id,
                             const QString &lbl = QString(),
                             DiagramNodeShape sh = DiagramNodeShape::Standard) {
        if (id.isEmpty() || id == QStringLiteral("[*]") || id == QStringLiteral("*")) return;
        if (!stateIds.contains(id)) {
            stateIds.append(id);
        }
        if (!lbl.isEmpty() && (!stateLabels.contains(id) || stateLabels[id] == id)) {
            stateLabels[id] = lbl;
        } else if (!stateLabels.contains(id)) {
            stateLabels[id] = id;
        }
        if (sh != DiagramNodeShape::Standard || !stateShapes.contains(id)) {
            stateShapes[id] = sh;
        }
    };

    if (!def.initialState.isEmpty()) {
        registerState(def.initialState, QString(),
                      isFlowDiagram ? DiagramNodeShape::StartEnd : DiagramNodeShape::Standard);
    }
    for (const DiagramNode &n : def.nodes) {
        registerState(n.id, n.label, n.shape);
    }
    for (const DiagramEdge &e : def.edges) {
        registerState(e.from);
        registerState(e.to);
    }
    for (const DiagramEvent &ev : events) {
        registerState(ev.from);
        registerState(ev.to);
    }

    QList<QPair<QString, QString>> allEdges;
    QMap<QPair<QString, QString>, QString> staticEdgeLabels;
    for (const DiagramEdge &e : def.edges) {
        const QPair<QString, QString> key(e.from, e.to);
        if (!allEdges.contains(key)) {
            allEdges.append(key);
        }
        staticEdgeLabels[key] = e.label;
    }
    for (const DiagramEvent &ev : events) {
        const QPair<QString, QString> key(ev.from, ev.to);
        if (!ev.from.isEmpty() && !ev.to.isEmpty() && !allEdges.contains(key)) {
            allEdges.append(key);
        }
    }

    const QMap<QString, QPointF> positions = computeLayeredLayout(def, stateIds, allEdges, isFlowDiagram);

    QPair<QString, QString> activeEdgeKey;
    int activeEdgeLogIdx = -1;
    if (snapshot.activeEventIndex >= 0 && snapshot.activeEventIndex < events.size()) {
        const DiagramEvent &actEv = events[snapshot.activeEventIndex];
        activeEdgeKey = qMakePair(actEv.from, actEv.to);
        activeEdgeLogIdx = actEv.dltIndex;
    }

    // Draw initial [*] marker for State Diagram
    if (!isFlowDiagram && !def.initialState.isEmpty() && positions.contains(def.initialState)) {
        const QPointF initCenter = positions.value(def.initialState);
        const QPointF startPt(initCenter.x() - kStateNodeWidth / 2.0 - 55.0, initCenter.y());
        const QPointF endPt(initCenter.x() - kStateNodeWidth / 2.0 - 2.0, initCenter.y());

        QGraphicsEllipseItem *startDot = m_scene->addEllipse(
            startPt.x() - 9.0, startPt.y() - 9.0, 18.0, 18.0,
            QPen(QColor(30, 41, 59), 2.0), QBrush(QColor(30, 41, 59)));
        startDot->setToolTip(QStringLiteral("Initial State: %1").arg(def.initialState));

        QPainterPath initPath(startPt + QPointF(9.0, 0.0));
        initPath.lineTo(endPt);
        m_scene->addPath(initPath, QPen(QColor(30, 41, 59), 2.0));
        addArrowHead(m_scene, endPt, startPt, QColor(30, 41, 59), -1, QString());
    }

    // Draw Transitions / Flow Edges
    const QSet<QPair<QString, QString>> edgeSet(allEdges.begin(), allEdges.end());

    for (const QPair<QString, QString> &edgeKey : allEdges) {
        const QString &from = edgeKey.first;
        const QString &to = edgeKey.second;
        if (!positions.contains(from) || !positions.contains(to)) {
            continue;
        }

        const bool isActiveEdge = (edgeKey == activeEdgeKey && activeEdgeLogIdx >= 0);
        const int visitCount = snapshot.transitionCounts.value(edgeKey, 0);
        const int lastLogIdx = snapshot.transitionLastLogIndex.value(edgeKey, -1);

        QColor edgeColor = QColor(148, 163, 184);
        qreal penWidth = 1.6;
        Qt::PenStyle penStyle = Qt::DashLine;

        if (isActiveEdge) {
            edgeColor = snapshot.exactMatchAtIndex ? QColor(217, 119, 6) : QColor(5, 150, 105);
            penWidth = 3.2;
            penStyle = Qt::SolidLine;
        } else if (visitCount > 0) {
            edgeColor = QColor(37, 99, 235);
            penWidth = 2.2;
            penStyle = Qt::SolidLine;
        } else if (staticEdgeLabels.contains(edgeKey)) {
            edgeColor = QColor(100, 116, 139);
            penStyle = Qt::SolidLine;
        }

        QString displayLabel = snapshot.transitionLastLabel.value(edgeKey, staticEdgeLabels.value(edgeKey));
        if (visitCount > 0) {
            displayLabel = displayLabel.isEmpty()
                               ? QStringLiteral("x%1 (#%2)").arg(visitCount).arg(lastLogIdx)
                               : QStringLiteral("%1 [x%2, #%3]").arg(displayLabel).arg(visitCount).arg(lastLogIdx);
        }

        const QString tooltip = (lastLogIdx >= 0)
                                    ? QStringLiteral("%1 -> %2\nEvent/Flow: %3\nCount: %4\nLast DLT Index: #%5 (Click to jump)")
                                          .arg(from, to, displayLabel).arg(visitCount).arg(lastLogIdx)
                                    : QStringLiteral("%1 -> %2 (Not triggered up to current log)").arg(from, to);

        const QPointF p1 = positions.value(from);
        const QPointF p2 = positions.value(to);

        QPainterPath path;
        QPointF labelPos;
        QPointF arrowTip;
        QPointF arrowFromDir;

        if (from == to) {
            const QPointF startPt(p1.x() - 28.0, p1.y() - kStateNodeHeight / 2.0);
            const QPointF endPt(p1.x() + 28.0, p1.y() - kStateNodeHeight / 2.0);
            const QPointF ctrl1(p1.x() - 55.0, p1.y() - kStateNodeHeight / 2.0 - 55.0);
            const QPointF ctrl2(p1.x() + 55.0, p1.y() - kStateNodeHeight / 2.0 - 55.0);
            path.moveTo(startPt);
            path.cubicTo(ctrl1, ctrl2, endPt);
            labelPos = QPointF(p1.x(), p1.y() - kStateNodeHeight / 2.0 - 46.0);
            arrowTip = endPt;
            arrowFromDir = ctrl2;
        } else {
            const bool hasReverseEdge = edgeSet.contains(qMakePair(to, from));
            const QPointF delta = p2 - p1;
            const qreal dist = std::hypot(delta.x(), delta.y());
            const QPointF unit = (dist > 1e-3) ? (delta / dist) : QPointF(1.0, 0.0);
            const QPointF normal(-unit.y(), unit.x());

            const qreal curveOffset = hasReverseEdge ? 34.0 : 0.0;
            const QPointF midPt = (p1 + p2) * 0.5 + normal * curveOffset;

            const QPointF startPt = intersectNodeRect(p1, midPt, kStateNodeWidth / 2.0 + 2.0, kStateNodeHeight / 2.0 + 2.0);
            const QPointF endPt = intersectNodeRect(p2, midPt, kStateNodeWidth / 2.0 + 4.0, kStateNodeHeight / 2.0 + 4.0);

            path.moveTo(startPt);
            if (qAbs(curveOffset) > 1.0) {
                path.quadTo(midPt, endPt);
                labelPos = (startPt + 2.0 * midPt + endPt) * 0.25;
                arrowFromDir = midPt;
            } else {
                path.lineTo(endPt);
                labelPos = (startPt + endPt) * 0.5 + normal * 12.0;
                arrowFromDir = startPt;
            }
            arrowTip = endPt;
        }

        QPen edgePen(edgeColor, penWidth, penStyle, Qt::RoundCap, Qt::RoundJoin);
        QGraphicsPathItem *pathItem = m_scene->addPath(path, edgePen);
        pathItem->setZValue(isActiveEdge ? 2.5 : 1.0);
        pathItem->setToolTip(tooltip);
        if (lastLogIdx >= 0) {
            pathItem->setData(kDataKeyLogIndex, lastLogIdx);
            pathItem->setCursor(Qt::PointingHandCursor);
        }

        addArrowHead(m_scene, arrowTip, arrowFromDir, edgeColor, lastLogIdx, tooltip);

        if (!displayLabel.isEmpty()) {
            QFont lblFont(QStringLiteral("Segoe UI"), 8, isActiveEdge ? QFont::Bold : QFont::Normal);
            QGraphicsSimpleTextItem *txtItem = new QGraphicsSimpleTextItem(displayLabel);
            txtItem->setFont(lblFont);
            const QRectF txtRect = txtItem->boundingRect();

            QGraphicsRectItem *pill = m_scene->addRect(
                labelPos.x() - txtRect.width() / 2.0 - 5.0,
                labelPos.y() - txtRect.height() / 2.0 - 2.0,
                txtRect.width() + 10.0,
                txtRect.height() + 4.0,
                QPen(isActiveEdge ? edgeColor : QColor(203, 213, 225), isActiveEdge ? 1.5 : 1.0),
                QBrush(isActiveEdge ? QColor(254, 243, 199) : QColor(255, 255, 255, 235)));
            pill->setZValue(isActiveEdge ? 3.5 : 2.0);
            pill->setToolTip(tooltip);
            if (lastLogIdx >= 0) {
                pill->setData(kDataKeyLogIndex, lastLogIdx);
                pill->setCursor(Qt::PointingHandCursor);
            }

            txtItem->setParentItem(pill);
            txtItem->setPos(labelPos.x() - txtRect.width() / 2.0, labelPos.y() - txtRect.height() / 2.0);
            txtItem->setBrush(isActiveEdge ? QColor(146, 64, 14) : QColor(51, 65, 85));
        }
    }

    // Draw State / Flow Step Nodes
    for (const QString &stateId : stateIds) {
        if (!positions.contains(stateId)) {
            continue;
        }
        const QPointF center = positions.value(stateId);
        const QRectF nodeRect(-kStateNodeWidth / 2.0, -kStateNodeHeight / 2.0, kStateNodeWidth, kStateNodeHeight);

        const bool isCurrent = (stateId == snapshot.currentState);
        const int visits = snapshot.stateVisitCounts.value(stateId, 0);
        const int lastLogIdx = snapshot.stateLastLogIndex.value(stateId, -1);
        const QString lastTime = snapshot.stateLastTime.value(stateId);
        const DiagramNodeShape shape = stateShapes.value(stateId, DiagramNodeShape::Standard);

        QColor borderColor(148, 163, 184);
        QColor fillTop(248, 250, 252);
        QColor fillBottom(241, 245, 249);
        QColor titleColor(71, 85, 105);
        QColor subColor(100, 116, 139);
        qreal borderWidth = 1.5;
        Qt::PenStyle borderStyle = Qt::DashLine;

        if (isCurrent) {
            borderColor = QColor(4, 120, 87);
            fillTop = QColor(16, 185, 129);
            fillBottom = QColor(5, 150, 105);
            titleColor = QColor(255, 255, 255);
            subColor = QColor(209, 250, 229);
            borderWidth = 3.2;
            borderStyle = Qt::SolidLine;
        } else if (visits > 0) {
            borderColor = QColor(37, 99, 235);
            fillTop = QColor(239, 246, 255);
            fillBottom = QColor(219, 234, 254);
            titleColor = QColor(30, 58, 138);
            subColor = QColor(37, 99, 235);
            borderWidth = 2.0;
            borderStyle = Qt::SolidLine;
        }

        QPainterPath shapePath;
        if (isFlowDiagram && shape == DiagramNodeShape::Decision) {
            // Diamond shape for Flowchart condition / decision
            QPolygonF diamond;
            diamond << QPointF(0.0, -kStateNodeHeight / 2.0 - 8.0)
                    << QPointF(kStateNodeWidth / 2.0 + 14.0, 0.0)
                    << QPointF(0.0, kStateNodeHeight / 2.0 + 8.0)
                    << QPointF(-kStateNodeWidth / 2.0 - 14.0, 0.0);
            diamond << diamond.first();
            shapePath.addPolygon(diamond);
        } else if (isFlowDiagram && shape == DiagramNodeShape::StartEnd) {
            // Pill / stadium shape for Flowchart start/end
            shapePath.addRoundedRect(nodeRect, kStateNodeHeight / 2.0, kStateNodeHeight / 2.0);
        } else if (isFlowDiagram) {
            // Crisp process rectangle for Flowchart step
            shapePath.addRoundedRect(nodeRect, 4.0, 4.0);
        } else {
            // Smooth rounded rectangle for State Diagram
            shapePath.addRoundedRect(nodeRect, 14.0, 14.0);
        }

        QLinearGradient grad(nodeRect.topLeft(), nodeRect.bottomLeft());
        grad.setColorAt(0.0, fillTop);
        grad.setColorAt(1.0, fillBottom);

        QGraphicsPathItem *nodeItem = m_scene->addPath(
            shapePath,
            QPen(borderColor, borderWidth, borderStyle),
            QBrush(grad));
        nodeItem->setPos(center);
        nodeItem->setZValue(isCurrent ? 5.0 : 4.0);
        nodeItem->setData(kDataKeyStateId, stateId);
        if (lastLogIdx >= 0) {
            nodeItem->setData(kDataKeyLogIndex, lastLogIdx);
            nodeItem->setCursor(Qt::PointingHandCursor);
        }

        if (isFlowDiagram && shape == DiagramNodeShape::Subroutine) {
            // Inner vertical bars for subroutine node
            QGraphicsLineItem *lBar = new QGraphicsLineItem(
                nodeRect.left() + 10.0, nodeRect.top(), nodeRect.left() + 10.0, nodeRect.bottom(), nodeItem);
            lBar->setPen(QPen(borderColor, borderWidth));
            QGraphicsLineItem *rBar = new QGraphicsLineItem(
                nodeRect.right() - 10.0, nodeRect.top(), nodeRect.right() - 10.0, nodeRect.bottom(), nodeItem);
            rBar->setPen(QPen(borderColor, borderWidth));
        }

        const QString kindName = isFlowDiagram ? QStringLiteral("Flow Step") : QStringLiteral("State");
        const QString tooltip = QStringLiteral("%1: %2 (%3)\nStatus: %4\nVisits up to log: %5\nLast Entry Log: %6\nTime: %7")
                                    .arg(kindName,
                                         stateLabels.value(stateId, stateId),
                                         stateId,
                                         isCurrent ? QStringLiteral("CURRENT ACTIVE") : (visits > 0 ? QStringLiteral("Visited") : QStringLiteral("Not Visited")),
                                         QString::number(visits),
                                         lastLogIdx >= 0 ? QStringLiteral("#%1 (Click to jump)").arg(lastLogIdx) : QStringLiteral("Initial / None"),
                                         lastTime.isEmpty() ? QStringLiteral("-") : lastTime);
        nodeItem->setToolTip(tooltip);

        if (isCurrent) {
            const QString badgeText = isFlowDiagram
                                          ? (snapshot.exactMatchAtIndex ? QStringLiteral("★ STEP EXECUTED") : QStringLiteral("▶ ACTIVE STEP"))
                                          : (snapshot.exactMatchAtIndex ? QStringLiteral("★ TRANSITIONED HERE") : QStringLiteral("● CURRENT STATE"));
            QFont badgeFont(QStringLiteral("Segoe UI"), 7, QFont::Bold);
            QGraphicsSimpleTextItem *badgeTxt = new QGraphicsSimpleTextItem(badgeText);
            badgeTxt->setFont(badgeFont);
            const QRectF bRect = badgeTxt->boundingRect();

            const qreal badgeY = (isFlowDiagram && shape == DiagramNodeShape::Decision)
                                     ? (nodeRect.top() - 19.0)
                                     : (nodeRect.top() - 11.0);
            QGraphicsRectItem *badgeBg = new QGraphicsRectItem(
                -bRect.width() / 2.0 - 7.0,
                badgeY,
                bRect.width() + 14.0,
                18.0,
                nodeItem);
            badgeBg->setBrush(snapshot.exactMatchAtIndex ? QColor(245, 158, 11) : QColor(6, 95, 70));
            badgeBg->setPen(QPen(Qt::white, 1.2));

            badgeTxt->setParentItem(badgeBg);
            badgeTxt->setBrush(Qt::white);
            badgeTxt->setPos(-bRect.width() / 2.0, badgeY + (18.0 - bRect.height()) / 2.0);
        }

        QString titleStr = stateLabels.value(stateId, stateId);
        if (titleStr.length() > 19) {
            titleStr = titleStr.left(17) + QStringLiteral("..");
        }
        QFont titleFont(QStringLiteral("Segoe UI"), 9, QFont::Bold);
        QGraphicsSimpleTextItem *titleTxt = new QGraphicsSimpleTextItem(titleStr, nodeItem);
        titleTxt->setFont(titleFont);
        titleTxt->setBrush(titleColor);
        const QRectF tRect = titleTxt->boundingRect();
        titleTxt->setPos(-tRect.width() / 2.0, -tRect.height() + 2.0);

        QString subStr;
        if (lastLogIdx >= 0) {
            subStr = QStringLiteral("Log #%1 • Visits: %2").arg(lastLogIdx).arg(visits);
        } else if (visits > 0) {
            subStr = isFlowDiagram ? QStringLiteral("Start Step") : QStringLiteral("Initial State");
        } else {
            subStr = isFlowDiagram ? QStringLiteral("Pending") : QStringLiteral("Unvisited");
        }
        QFont subFont(QStringLiteral("Segoe UI"), 8);
        QGraphicsSimpleTextItem *subTxt = new QGraphicsSimpleTextItem(subStr, nodeItem);
        subTxt->setFont(subFont);
        subTxt->setBrush(subColor);
        const QRectF sRect = subTxt->boundingRect();
        subTxt->setPos(-sRect.width() / 2.0, 5.0);
    }
}

void DiagramGraphicsView::renderSequenceDiagram(const DiagramDefinition &def,
                                                const QVector<DiagramEvent> &events,
                                                const DiagramSnapshot &snapshot)
{
    QStringList participants;
    QMap<QString, QString> partLabels;
    auto registerPart = [&](const QString &id, const QString &lbl = QString()) {
        if (id.isEmpty() || id == QStringLiteral("*")) return;
        if (!participants.contains(id)) {
            participants.append(id);
        }
        if (!lbl.isEmpty() && (!partLabels.contains(id) || partLabels[id] == id)) {
            partLabels[id] = lbl;
        } else if (!partLabels.contains(id)) {
            partLabels[id] = id;
        }
    };

    for (const DiagramNode &n : def.nodes) {
        registerPart(n.id, n.label);
    }
    for (const DiagramEdge &e : def.edges) {
        registerPart(e.from);
        registerPart(e.to);
    }
    for (const DiagramEvent &ev : events) {
        registerPart(ev.from);
        registerPart(ev.to);
    }

    if (participants.isEmpty()) {
        QGraphicsSimpleTextItem *emptyTxt = m_scene->addSimpleText(
            QStringLiteral("No sequence participants or matching DLT events yet."));
        emptyTxt->setBrush(QColor(100, 116, 139));
        return;
    }

    int startRow = 0;
    int endRow = events.size() - 1;
    const int kMaxVisibleRows = 120;
    if (events.size() > kMaxVisibleRows) {
        const int centerIdx = (snapshot.activeEventIndex >= 0) ? snapshot.activeEventIndex : (events.size() - 1);
        startRow = qMax(0, centerIdx - kMaxVisibleRows / 2);
        endRow = qMin(events.size() - 1, startRow + kMaxVisibleRows - 1);
        if (endRow - startRow + 1 < kMaxVisibleRows) {
            startRow = qMax(0, endRow - kMaxVisibleRows + 1);
        }
    }

    const int numRows = events.isEmpty() ? def.edges.size() : (endRow - startRow + 1);
    const qreal leftMargin = 150.0;
    const qreal colSpacing = 185.0;
    const qreal headerY = 20.0;
    const qreal headerW = 135.0;
    const qreal headerH = 44.0;
    const qreal firstRowY = 95.0;
    const qreal rowStep = 46.0;
    const qreal totalHeight = firstRowY + qMax(1, numRows) * rowStep + 50.0;

    QMap<QString, qreal> partX;
    for (int i = 0; i < participants.size(); ++i) {
        const QString &pId = participants[i];
        const qreal x = leftMargin + i * colSpacing;
        partX[pId] = x;

        QGraphicsRectItem *topBox = m_scene->addRect(
            x - headerW / 2.0, headerY, headerW, headerH,
            QPen(QColor(30, 64, 175), 2.0),
            QBrush(QColor(239, 246, 255)));
        topBox->setZValue(3.0);

        QString titleStr = partLabels.value(pId, pId);
        if (titleStr.length() > 16) titleStr = titleStr.left(14) + QStringLiteral("..");
        QFont pFont(QStringLiteral("Segoe UI"), 9, QFont::Bold);
        QGraphicsSimpleTextItem *pTxt = m_scene->addSimpleText(titleStr, pFont);
        pTxt->setBrush(QColor(30, 58, 138));
        const QRectF r = pTxt->boundingRect();
        pTxt->setPos(x - r.width() / 2.0, headerY + (headerH - r.height()) / 2.0);
        pTxt->setZValue(4.0);

        QGraphicsLineItem *line = m_scene->addLine(
            x, headerY + headerH, x, totalHeight,
            QPen(QColor(148, 163, 184), 1.5, Qt::DashLine));
        line->setZValue(0.5);
    }

    const qreal fullRowWidth = leftMargin + (participants.size() - 1) * colSpacing + headerW / 2.0 + 30.0;

    if (events.isEmpty()) {
        for (int i = 0; i < def.edges.size(); ++i) {
            const DiagramEdge &edge = def.edges[i];
            const qreal y = firstRowY + i * rowStep;
            const qreal x1 = partX.value(edge.from, leftMargin);
            const qreal x2 = partX.value(edge.to, leftMargin + colSpacing);

            QPen pen(QColor(100, 116, 139), 1.8,
                     edge.arrowStyle == QStringLiteral("reply") ? Qt::DashLine : Qt::SolidLine);
            m_scene->addLine(x1, y, x2, y, pen);
            addArrowHead(m_scene, QPointF(x2, y), QPointF(x1, y), QColor(100, 116, 139), -1, QString());

            QGraphicsSimpleTextItem *lbl = m_scene->addSimpleText(
                edge.label, QFont(QStringLiteral("Segoe UI"), 8));
            lbl->setBrush(QColor(71, 85, 105));
            const QRectF lr = lbl->boundingRect();
            lbl->setPos((x1 + x2) / 2.0 - lr.width() / 2.0, y - lr.height() - 3.0);
        }
        return;
    }

    for (int i = startRow; i <= endRow; ++i) {
        const DiagramEvent &ev = events[i];
        const int localRow = i - startRow;
        const qreal y = firstRowY + localRow * rowStep;

        const bool isActiveRow = (i == snapshot.activeEventIndex);
        const bool isFutureRow = (snapshot.activeEventIndex >= 0 && i > snapshot.activeEventIndex);

        if (isActiveRow) {
            const QColor bannerFill = snapshot.exactMatchAtIndex
                                          ? QColor(254, 243, 199)
                                          : QColor(209, 250, 229);
            const QColor bannerBorder = snapshot.exactMatchAtIndex
                                            ? QColor(245, 158, 11)
                                            : QColor(16, 185, 129);
            QGraphicsRectItem *banner = m_scene->addRect(
                10.0, y - 22.0, fullRowWidth, rowStep - 4.0,
                QPen(bannerBorder, 1.8),
                QBrush(bannerFill));
            banner->setZValue(0.8);
            banner->setData(kDataKeyLogIndex, ev.dltIndex);
            banner->setCursor(Qt::PointingHandCursor);
        }

        const QString indexTag = QStringLiteral("%1#%2  %3")
                                     .arg(isActiveRow ? QStringLiteral("▶ ") : QString())
                                     .arg(ev.dltIndex)
                                     .arg(ev.timeString.right(12));
        QGraphicsSimpleTextItem *gutterTxt = m_scene->addSimpleText(
            indexTag, QFont(QStringLiteral("Consolas"), 8, isActiveRow ? QFont::Bold : QFont::Normal));
        gutterTxt->setBrush(isActiveRow ? QColor(6, 95, 70)
                                        : (isFutureRow ? QColor(148, 163, 184) : QColor(71, 85, 105)));
        gutterTxt->setPos(16.0, y - 10.0);
        gutterTxt->setZValue(2.0);
        gutterTxt->setData(kDataKeyLogIndex, ev.dltIndex);
        gutterTxt->setCursor(Qt::PointingHandCursor);

        const qreal x1 = partX.value(ev.from, leftMargin);
        const qreal x2 = partX.value(ev.to, leftMargin);

        const QColor arrowColor = isActiveRow
                                      ? (snapshot.exactMatchAtIndex ? QColor(180, 83, 9) : QColor(4, 120, 87))
                                      : (isFutureRow ? QColor(148, 163, 184) : QColor(37, 99, 235));
        const qreal arrowWidth = isActiveRow ? 2.8 : 1.8;
        const Qt::PenStyle arrowStyle = (ev.arrowStyle == QStringLiteral("reply") || isFutureRow)
                                            ? Qt::DashLine
                                            : Qt::SolidLine;

        const QString tooltip = QStringLiteral("DLT Index: #%1\nTime: %2\n%3 -> %4\nMessage: %5\nPayload: %6\n(Click to jump to log)")
                                    .arg(ev.dltIndex)
                                    .arg(ev.timeString, ev.from, ev.to, ev.label, ev.payload);

        if (ev.from == ev.to) {
            QPainterPath loopPath(QPointF(x1, y - 6.0));
            loopPath.lineTo(x1 + 42.0, y - 6.0);
            loopPath.lineTo(x1 + 42.0, y + 8.0);
            loopPath.lineTo(x1, y + 8.0);
            QGraphicsPathItem *pItem = m_scene->addPath(loopPath, QPen(arrowColor, arrowWidth, arrowStyle));
            pItem->setZValue(2.0);
            pItem->setData(kDataKeyLogIndex, ev.dltIndex);
            pItem->setCursor(Qt::PointingHandCursor);
            pItem->setToolTip(tooltip);
            addArrowHead(m_scene, QPointF(x1, y + 8.0), QPointF(x1 + 42.0, y + 8.0), arrowColor, ev.dltIndex, tooltip);
        } else {
            QGraphicsLineItem *lItem = m_scene->addLine(x1, y, x2, y, QPen(arrowColor, arrowWidth, arrowStyle));
            lItem->setZValue(2.0);
            lItem->setData(kDataKeyLogIndex, ev.dltIndex);
            lItem->setCursor(Qt::PointingHandCursor);
            lItem->setToolTip(tooltip);
            addArrowHead(m_scene, QPointF(x2, y), QPointF(x1, y), arrowColor, ev.dltIndex, tooltip);
        }

        QString msgText = ev.label;
        if (msgText.length() > 42) {
            msgText = msgText.left(40) + QStringLiteral("...");
        }
        QGraphicsSimpleTextItem *msgItem = m_scene->addSimpleText(
            msgText, QFont(QStringLiteral("Segoe UI"), 8, isActiveRow ? QFont::Bold : QFont::Normal));
        msgItem->setBrush(isActiveRow ? QColor(15, 23, 42)
                                      : (isFutureRow ? QColor(148, 163, 184) : QColor(30, 41, 59)));
        const QRectF mr = msgItem->boundingRect();
        const qreal midX = (ev.from == ev.to) ? (x1 + 24.0 + mr.width() / 2.0) : ((x1 + x2) * 0.5);
        msgItem->setPos(midX - mr.width() / 2.0, y - mr.height() - 3.0);
        msgItem->setZValue(2.5);
        msgItem->setData(kDataKeyLogIndex, ev.dltIndex);
        msgItem->setCursor(Qt::PointingHandCursor);
        msgItem->setToolTip(tooltip);
    }
}

} // namespace DltDiagram
