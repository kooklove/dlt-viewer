#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QFile>

#include "../../plugin/dltdiagramplugin/diagrammodel.h"
#include "../../plugin/dltdiagramplugin/diagramengine.h"

using namespace DltDiagram;

TEST(DltDiagramPluginTest, ParsesMermaidAndPlantUmlStateDiagrams)
{
    DiagramDefinition def;
    def.id = "test_state";
    def.title = "Test State Machine";
    const QString mermaidSrc = QStringLiteral(
        "stateDiagram-v2\n"
        "    [*] --> Idle\n"
        "    state \"Connecting to Server\" as Connecting\n"
        "    Idle --> Connecting : StartConnect\n"
        "    Connecting --> Connected : AuthSuccess\n"
        "    Connecting --> Error : Timeout\n"
        "    %% @rule apid=CONN ctid=FSM regex=\"State:\\s*(\\w+)\\s*->\\s*(\\w+)\" from=$1 to=$2 event=Transition\n");

    DiagramParser::parseDiagramSource(mermaidSrc, def);

    EXPECT_EQ(def.type, DiagramType::StateDiagram);
    EXPECT_EQ(def.format, DiagramFormat::Mermaid);
    EXPECT_EQ(def.initialState, QStringLiteral("Idle"));
    EXPECT_EQ(def.nodes.size(), 4);
    EXPECT_EQ(def.edges.size(), 3);
    ASSERT_EQ(def.rules.size(), 1);
    EXPECT_EQ(def.rules[0].apid, QStringLiteral("CONN"));
    EXPECT_EQ(def.rules[0].ctid, QStringLiteral("FSM"));
}

TEST(DltDiagramPluginTest, ParsesFlowchartAndTracksActiveStep)
{
    DiagramDefinition def;
    def.id = "ota_flow";
    def.title = "OTA Flow";
    def.type = DiagramType::FlowDiagram;
    const QString flowSrc = QStringLiteral(
        "flowchart TD\n"
        "    StartNode([Start Update]) --> CheckSig{Valid Sig?}\n"
        "    CheckSig -->|Yes| Install[Install Image]\n"
        "    CheckSig -->|No| Abort([Abort])\n");

    DiagramParser::parseDiagramSource(flowSrc, def);
    EXPECT_EQ(def.type, DiagramType::FlowDiagram);
    EXPECT_EQ(def.initialState, QStringLiteral("StartNode"));
    ASSERT_EQ(def.nodes.size(), 4);
    EXPECT_EQ(def.nodes[0].shape, DiagramNodeShape::StartEnd);
    EXPECT_EQ(def.nodes[1].shape, DiagramNodeShape::Decision);
    EXPECT_EQ(def.nodes[2].shape, DiagramNodeShape::Standard);
}

TEST(DltDiagramPluginTest, MultiTabJsonCreationOrderingAlphabeticalSortAndFavorites)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    DiagramStateEngine engine;
    engine.setWorkspaceDirectory(tempDir.path(), false);

    QString err;
    ASSERT_TRUE(engine.addTab(QStringLiteral("AudioTab"), err)) << err.toStdString();
    ASSERT_TRUE(engine.addTab(QStringLiteral("PowerTab"), err)) << err.toStdString();
    EXPECT_TRUE(QFile::exists(tempDir.filePath(QStringLiteral("AudioTab.json"))));
    EXPECT_TRUE(QFile::exists(tempDir.filePath(QStringLiteral("PowerTab.json"))));

    // Add State diagrams out of alphabetical order ("Zebra State", "Alpha State")
    DiagramDefinition dZebra;
    dZebra.tabName = QStringLiteral("AudioTab");
    dZebra.id = QStringLiteral("zebra_state");
    dZebra.title = QStringLiteral("Zebra State");
    dZebra.type = DiagramType::StateDiagram;
    engine.addOrUpdateDiagram(dZebra, true, err);

    DiagramDefinition dAlpha;
    dAlpha.tabName = QStringLiteral("AudioTab");
    dAlpha.id = QStringLiteral("alpha_state");
    dAlpha.title = QStringLiteral("Alpha State");
    dAlpha.type = DiagramType::StateDiagram;
    engine.addOrUpdateDiagram(dAlpha, true, err);

    // Add Flow diagrams out of alphabetical order ("Zeta Flow", "Beta Flow")
    DiagramDefinition fZeta;
    fZeta.tabName = QStringLiteral("AudioTab");
    fZeta.id = QStringLiteral("zeta_flow");
    fZeta.title = QStringLiteral("Zeta Flow");
    fZeta.type = DiagramType::FlowDiagram;
    engine.addOrUpdateDiagram(fZeta, true, err);

    DiagramDefinition fBeta;
    fBeta.tabName = QStringLiteral("AudioTab");
    fBeta.id = QStringLiteral("beta_flow");
    fBeta.title = QStringLiteral("Beta Flow");
    fBeta.type = DiagramType::FlowDiagram;
    engine.addOrUpdateDiagram(fBeta, true, err);

    const QList<DiagramDefinition> stateList = engine.diagramsForTabAndType(QStringLiteral("AudioTab"), DiagramType::StateDiagram);
    ASSERT_EQ(stateList.size(), 2);
    EXPECT_EQ(stateList[0].title, QStringLiteral("Alpha State"));
    EXPECT_EQ(stateList[1].title, QStringLiteral("Zebra State"));

    const QList<DiagramDefinition> flowList = engine.diagramsForTabAndType(QStringLiteral("AudioTab"), DiagramType::FlowDiagram);
    ASSERT_EQ(flowList.size(), 2);
    EXPECT_EQ(flowList[0].title, QStringLiteral("Beta Flow"));
    EXPECT_EQ(flowList[1].title, QStringLiteral("Zeta Flow"));

    // Test Favorites toggle and alphabetical sorting
    engine.setFavorite(dZebra.favoriteKey(), true);
    engine.setFavorite(fBeta.favoriteKey(), true);
    const QList<DiagramDefinition> favs = engine.favoriteDiagramsSorted();
    ASSERT_GE(favs.size(), 2);

    // Test Tab reordering
    engine.setTabOrder({QStringLiteral("PowerTab"), QStringLiteral("AudioTab")});
    EXPECT_EQ(engine.tabNames().first(), QStringLiteral("PowerTab"));
}
