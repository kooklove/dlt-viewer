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
 * \file diagramgraphicsview.h
 * @licence end@
 */

#ifndef DIAGRAMGRAPHICSVIEW_H
#define DIAGRAMGRAPHICSVIEW_H

#include "diagrammodel.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPushButton>
#include <QMap>
#include <QPointF>

namespace DltDiagram {

class DiagramGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DiagramGraphicsView(QWidget *parent = nullptr);
    ~DiagramGraphicsView() override = default;

    void renderDiagram(const DiagramDefinition &def,
                       const QVector<DiagramEvent> &events,
                       const DiagramSnapshot &snapshot);
    void clearCanvasWithMessage(const QString &message);

    void resetCustomLayout();
    void zoomInView();
    void zoomOutView();
    void fitDiagramInView();

    bool isAutoFitEnabled() const { return m_autoFitEnabled; }
    void setAutoFitEnabled(bool enabled);

signals:
    void jumpToLogRequested(int dltIndex);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupOverlayControls();
    void repositionOverlayControls();
    void updateAutoFitButtonStyle();

    void renderStateOrFlowDiagram(const DiagramDefinition &def,
                                  const QVector<DiagramEvent> &events,
                                  const DiagramSnapshot &snapshot,
                                  bool isFlowDiagram);

    void renderSequenceDiagram(const DiagramDefinition &def,
                               const QVector<DiagramEvent> &events,
                               const DiagramSnapshot &snapshot);

    QMap<QString, QPointF> computeLayeredLayout(const DiagramDefinition &def,
                                                const QStringList &stateIds,
                                                const QList<QPair<QString, QString>> &allEdges,
                                                bool topDownFlow) const;

    static void addArrowHead(QGraphicsScene *scene,
                             const QPointF &tip,
                             const QPointF &fromDir,
                             const QColor &color,
                             int logIndex,
                             const QString &tooltip);

    QGraphicsScene *m_scene = nullptr;
    QString m_currentDiagramKey;
    QMap<QString, QPointF> m_customNodePositions;
    QPoint m_pressPos;

    QWidget *m_overlayWidget = nullptr;
    QPushButton *m_btnZoomIn = nullptr;
    QPushButton *m_btnZoomOut = nullptr;
    QPushButton *m_btnAutoFit = nullptr;
    bool m_autoFitEnabled = true;
};

} // namespace DltDiagram

#endif // DIAGRAMGRAPHICSVIEW_H
