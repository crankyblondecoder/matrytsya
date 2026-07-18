#ifndef JSON_HIVE_LOADER_UNIT_TEST_H
#define JSON_HIVE_LOADER_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/GraphHandle.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/actions/PingAction.hpp"
#include "../../../graph/nodes/PingNode.hpp"
#include "../../../persist/HiveBuilder.hpp"
#include "../../../persist/HiveNodeDescriptor.hpp"
#include "../../../persist/HiveSurfaceDescriptor.hpp"
#include "../../../persist/PersistException.hpp"
#include "../../../persist/json/JsonHiveLoader.hpp"

#include <cstddef>
#include <string>

/**
 * A JSON document covering all seven concrete node types, an edge (with an action flag
 * restriction), and a strobe emitter registration parses into descriptors with the expected fields.
 */
TEST(JsonHiveLoaderTest, FullValidHive_AllSevenNodeTypesParsed)
{
	std::string json = R"({
		"name": "TestHive",
		"nodes": [
			{ "type": "PingNode", "name": "ping1", "edges": [ { "toNodeName": "ping2", "actionFlags": ["PING_GRAPH_ACTION"] } ] },
			{ "type": "PingNode", "name": "ping2" },
			{ "type": "TeleportNode", "name": "teleport1",
				"destination": { "hiveName": "otherHive", "nodeName": "otherNode" } },
			{ "type": "SceneRootNode", "name": "root1" },
			{ "type": "SceneGeometryNode", "name": "geom1",
				"vertexes": [ { "posn": [0, 0, 0] }, { "posn": [1, 0, 0] }, { "posn": [0, 1, 0] } ] },
			{ "type": "SceneGeometryScriptNode", "name": "geomScript1", "coreScript": "", "pokeScript": "",
				"vertexes": [ { "posn": [0, 0, 0], "colour": [255, 0, 0, 255] } ] },
			{ "type": "SceneTransformNode", "name": "xform1",
				"transform": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1] },
			{ "type": "SceneTransformScriptNode", "name": "xformScript1", "coreScript": "", "pokeScript": "" }
		],
		"strobeEmitters": [ { "nodeName": "root1", "frequencyHz": 10 } ]
	})";

	JsonHiveLoader loader(json);

	EXPECT_EQ(loader.getHiveName(), "TestHive");
	ASSERT_EQ(loader.getNodeCount(), 8u);

	HiveNodeDescriptor ping1 = loader.getNode(0);
	EXPECT_EQ(ping1.type, HiveNodeDescriptor::PING);
	EXPECT_EQ(ping1.name, "ping1");
	EXPECT_FALSE(ping1.pokeEnabled);
	ASSERT_EQ(ping1.edges.size(), 1u);
	EXPECT_EQ(ping1.edges[0].toNodeName, "ping2");
	ASSERT_EQ(ping1.edges[0].actionFlagNames.size(), 1u);
	EXPECT_EQ(ping1.edges[0].actionFlagNames[0], "PING_GRAPH_ACTION");

	HiveNodeDescriptor teleport1 = loader.getNode(2);
	EXPECT_EQ(teleport1.type, HiveNodeDescriptor::TELEPORT);
	EXPECT_EQ(teleport1.destination.getHiveName(), "otherHive");
	EXPECT_EQ(teleport1.destination.getNodeName(), "otherNode");
	EXPECT_EQ(teleport1.destination.getHostname(), "localhost");
	EXPECT_EQ(teleport1.destination.getPort(), 0);

	HiveNodeDescriptor geom1 = loader.getNode(4);
	EXPECT_EQ(geom1.type, HiveNodeDescriptor::SCENE_GEOMETRY);
	EXPECT_TRUE(geom1.hasVertexes);
	ASSERT_EQ(geom1.vertexes.size(), 3u);
	EXPECT_DOUBLE_EQ(geom1.vertexes[0].posn[0], 0.0);
	EXPECT_DOUBLE_EQ(geom1.vertexes[1].posn[0], 1.0);

	HiveNodeDescriptor geomScript1 = loader.getNode(5);
	EXPECT_EQ(geomScript1.type, HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT);
	EXPECT_EQ(geomScript1.coreScript, "");
	EXPECT_EQ(geomScript1.pokeScript, "");
	ASSERT_EQ(geomScript1.vertexes.size(), 1u);
	EXPECT_EQ(std::to_integer<int>(geomScript1.vertexes[0].colour[0]), 255);

	HiveNodeDescriptor xform1 = loader.getNode(6);
	EXPECT_EQ(xform1.type, HiveNodeDescriptor::SCENE_TRANSFORM);
	EXPECT_TRUE(xform1.hasTransform);
	EXPECT_DOUBLE_EQ(xform1.transform[0], 1.0);
	EXPECT_DOUBLE_EQ(xform1.transform[5], 1.0);

	HiveNodeDescriptor xformScript1 = loader.getNode(7);
	EXPECT_EQ(xformScript1.type, HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT);
	EXPECT_FALSE(xformScript1.hasTransform);
	EXPECT_EQ(xformScript1.coreScript, "");

	ASSERT_EQ(loader.getStrobeEmitterCount(), 1u);

	std::string strobeNodeName;
	unsigned strobeFrequencyHz;
	loader.getStrobeEmitter(0, strobeNodeName, strobeFrequencyHz);

	EXPECT_EQ(strobeNodeName, "root1");
	EXPECT_EQ(strobeFrequencyHz, 10u);
}

/**
 * A destination object that omits "hostname" defaults it to "localhost", per the schema.
 */
TEST(JsonHiveLoaderTest, TeleportNodeDestinationDefaultsHostnameToLocalhost)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "TeleportNode", "name": "t1",
				"destination": { "hiveName": "h", "nodeName": "n" } }
		]
	})";

	JsonHiveLoader loader(json);

	EXPECT_EQ(loader.getNode(0).destination.getHostname(), "localhost");
}

/**
 * A missing top level "name" member is rejected.
 */
TEST(JsonHiveLoaderTest, MissingTopLevelName_ThrowsJsonInvalidName)
{
	std::string json = R"({ "nodes": [] })";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * An empty "nodes" array is accepted by the loader itself; requiring at least one node is
 * HiveBuilder's business rule, not JsonHiveLoader's.
 */
TEST(JsonHiveLoaderTest, EmptyNodesArray_DoesNotThrowAtLoaderLevel)
{
	std::string json = R"({ "name": "Hive", "nodes": [] })";

	JsonHiveLoader loader(json);

	EXPECT_EQ(loader.getNodeCount(), 0u);
}

/**
 * Syntactically invalid JSON text is rejected.
 */
TEST(JsonHiveLoaderTest, MalformedJson_ThrowsJsonParseError)
{
	std::string json = "{ this is not valid json";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A node object missing the required "type" member is rejected.
 */
TEST(JsonHiveLoaderTest, NodeMissingType_ThrowsJsonInvalidNodeBase)
{
	std::string json = R"({ "name": "Hive", "nodes": [ { "name": "a" } ] })";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A node "type" string that doesn't match any known concrete node type is rejected.
 */
TEST(JsonHiveLoaderTest, UnrecognisedNodeType_ThrowsUnknownNodeType)
{
	std::string json = R"({ "name": "Hive", "nodes": [ { "type": "NotARealNodeType", "name": "a" } ] })";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A TeleportNode missing its required "destination" is rejected.
 */
TEST(JsonHiveLoaderTest, TeleportNodeMissingDestination_ThrowsJsonInvalidDestination)
{
	std::string json = R"({ "name": "Hive", "nodes": [ { "type": "TeleportNode", "name": "t1" } ] })";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A vertex object missing its required "posn" is rejected.
 */
TEST(JsonHiveLoaderTest, VertexMissingPosn_ThrowsJsonInvalidVertexes)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "SceneGeometryNode", "name": "g1", "vertexes": [ { "colour": [0, 0, 0, 255] } ] }
		]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A "transform" array with the wrong number of elements is rejected.
 */
TEST(JsonHiveLoaderTest, TransformWrongLength_ThrowsJsonInvalidTransform)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTransformNode", "name": "x1", "transform": [1, 0, 0] } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A script node missing its required "coreScript" is rejected.
 */
TEST(JsonHiveLoaderTest, ScriptNodeMissingCoreScript_ThrowsJsonInvalidScriptSource)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneGeometryScriptNode", "name": "g1", "pokeScript": "" } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A malformed strobeEmitters entry (missing "frequencyHz") is rejected.
 */
TEST(JsonHiveLoaderTest, StrobeEmitterMissingFrequencyHz_ThrowsJsonInvalidStrobeEmitters)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"strobeEmitters": [ { "nodeName": "root1" } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A JSON document with a GraphHiveSceneSurface bound to a SceneRootNode, and a strobeSurfaces
 * registration, parses into descriptors with the expected fields.
 */
TEST(JsonHiveLoaderTest, SurfacesAndStrobeSurfacesParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "type": "GraphHiveSceneSurface", "name": "surface1", "sceneRootNodeName": "root1" } ],
		"strobeSurfaces": [ { "surfaceName": "surface1", "frequencyHz": 10 } ]
	})";

	JsonHiveLoader loader(json);

	ASSERT_EQ(loader.getSurfaceCount(), 1u);

	HiveSurfaceDescriptor surface1 = loader.getSurface(0);
	EXPECT_EQ(surface1.type, HiveSurfaceDescriptor::SCENE_SURFACE);
	EXPECT_EQ(surface1.name, "surface1");
	EXPECT_EQ(surface1.sceneRootNodeName, "root1");

	ASSERT_EQ(loader.getStrobeSurfaceCount(), 1u);

	std::string strobeSurfaceName;
	unsigned strobeFrequencyHz;
	loader.getStrobeSurface(0, strobeSurfaceName, strobeFrequencyHz);

	EXPECT_EQ(strobeSurfaceName, "surface1");
	EXPECT_EQ(strobeFrequencyHz, 10u);
}

/**
 * A surface object missing the required "type" member is rejected.
 */
TEST(JsonHiveLoaderTest, SurfaceMissingType_ThrowsJsonInvalidSurfaces)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "name": "surface1", "sceneRootNodeName": "root1" } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A surface "type" string that doesn't match any known concrete surface type is rejected.
 */
TEST(JsonHiveLoaderTest, SurfaceUnrecognisedType_ThrowsUnknownSurfaceType)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "type": "NotARealSurfaceType", "name": "surface1", "sceneRootNodeName": "root1" } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A GraphHiveSceneSurface missing its required "sceneRootNodeName" is rejected.
 */
TEST(JsonHiveLoaderTest, SurfaceMissingSceneRootNode_ThrowsJsonInvalidSurfaces)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "type": "GraphHiveSceneSurface", "name": "surface1" } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A malformed strobeSurfaces entry (missing "frequencyHz") is rejected.
 */
TEST(JsonHiveLoaderTest, StrobeSurfaceMissingFrequencyHz_ThrowsJsonInvalidStrobeSurfaces)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "type": "GraphHiveSceneSurface", "name": "surface1", "sceneRootNodeName": "root1" } ],
		"strobeSurfaces": [ { "surfaceName": "surface1" } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * End-to-end: JSON parsed by JsonHiveLoader and built by HiveBuilder produces a hive whose edge is
 * actually wired (a ping emitted from the source node reaches the target node).
 */
TEST(JsonHiveLoaderTest, EndToEnd_JsonBuildsWorkingHive)
{
	std::string json = R"({
		"name": "TestHive",
		"nodes": [
			{ "type": "PingNode", "name": "source", "edges": [ { "toNodeName": "target" } ] },
			{ "type": "PingNode", "name": "target" }
		]
	})";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	GraphHandle<GraphHive> hiveHandle(hive);

	GraphHandle<GraphNode> sourceHandle = hive -> getNode("source");
	GraphHandle<GraphNode> targetHandle = hive -> getNode("target");

	ASSERT_TRUE(sourceHandle.isValid());
	ASSERT_TRUE(targetHandle.isValid());

	PingNode* sourceNode = dynamic_cast<PingNode*>(sourceHandle.getInstance());
	PingNode* targetNode = dynamic_cast<PingNode*>(targetHandle.getInstance());

	ASSERT_NE(sourceNode, nullptr);
	ASSERT_NE(targetNode, nullptr);

	PingAction* action = sourceNode -> emitPing(true);

	EXPECT_EQ(targetNode -> getPingCount(), 1u);

	action -> decrRef();

	hive -> shutdown();
}

/**
 * End-to-end: JSON parsed by JsonHiveLoader and built by HiveBuilder produces a hive whose surface
 * is actually attached and registered for strobing.
 */
TEST(JsonHiveLoaderTest, EndToEnd_JsonBuildsWorkingHiveWithSurface)
{
	std::string json = R"({
		"name": "TestHive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "type": "GraphHiveSceneSurface", "name": "surface1", "sceneRootNodeName": "root1" } ],
		"strobeSurfaces": [ { "surfaceName": "surface1", "frequencyHz": 10 } ]
	})";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	GraphHandle<GraphHive> hiveHandle(hive);

	EXPECT_TRUE(hive -> getSurface("surface1").isValid());

	hive -> shutdown();
}

#endif
