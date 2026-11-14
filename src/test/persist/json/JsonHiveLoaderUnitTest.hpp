#ifndef JSON_HIVE_LOADER_UNIT_TEST_H
#define JSON_HIVE_LOADER_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../util/Handle.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphHiveSceneSurface.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/actions/PingAction.hpp"
#include "../../../graph/actions/SceneAction.hpp"
#include "../../../graph/nodes/PingNode.hpp"
#include "../../../graph/nodes/SceneGeometry.hpp"
#include "../../../graph/nodes/SceneTextScriptNode.hpp"
#include "../../../persist/HiveBuilder.hpp"
#include "../../../persist/HiveNodeDescriptor.hpp"
#include "../../../persist/HiveSurfaceDescriptor.hpp"
#include "../../../persist/PersistException.hpp"
#include "../../../persist/json/JsonHiveLoader.hpp"

#include <cstddef>
#include <string>

/**
 * A JSON document covering all eight concrete node types, an edge (with an action flag
 * restriction), and a strobe emitter registration parses into descriptors with the expected fields.
 */
TEST(JsonHiveLoaderTest, FullValidHive_AllEightNodeTypesParsed)
{
	std::string json = R"({
		"name": "TestHive",
		"nodes": [
			{ "type": "PingNode", "name": "ping1", "edges": [ { "toNodeName": "ping2", "actionFlags": ["PING_GRAPH_ACTION"], "actionsCompleteAfterTraverse": true } ] },
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
			{ "type": "SceneTransformScriptNode", "name": "xformScript1", "coreScript": "", "pokeScript": "" },
			{ "type": "AgentNode", "name": "agent1", "capability": "HIGH",
				"prompts": [ { "nodeType": "PING_NODE", "prompt": "describe this node" } ] }
		],
		"strobeEmitters": [ { "nodeName": "root1", "periodMs": 100 } ]
	})";

	JsonHiveLoader loader(json);

	EXPECT_EQ(loader.getHiveName(), "TestHive");
	ASSERT_EQ(loader.getNodeCount(), 9u);

	HiveNodeDescriptor ping1 = loader.getNode(0);
	EXPECT_EQ(ping1.type, HiveNodeDescriptor::PING);
	EXPECT_EQ(ping1.name, "ping1");
	EXPECT_FALSE(ping1.pokeEnabled);
	ASSERT_EQ(ping1.edges.size(), 1u);
	EXPECT_EQ(ping1.edges[0].toNodeName, "ping2");
	ASSERT_EQ(ping1.edges[0].actionFlagNames.size(), 1u);
	EXPECT_EQ(ping1.edges[0].actionFlagNames[0], "PING_GRAPH_ACTION");
	EXPECT_TRUE(ping1.edges[0].actionsCompleteAfterTraverse);

	HiveNodeDescriptor teleport1 = loader.getNode(2);
	EXPECT_EQ(teleport1.type, HiveNodeDescriptor::TELEPORT);
	EXPECT_EQ(teleport1.destination.getHiveName(), "otherHive");
	EXPECT_EQ(teleport1.destination.getNodeName(), "otherNode");
	EXPECT_EQ(teleport1.destination.getHostname(), "localhost");
	EXPECT_EQ(teleport1.destination.getPort(), 0);

	HiveNodeDescriptor geom1 = loader.getNode(4);
	EXPECT_EQ(geom1.type, HiveNodeDescriptor::SCENE_GEOMETRY);
	ASSERT_EQ(geom1.vertexGroups.size(), 1u);
	EXPECT_EQ(geom1.vertexGroups[0].visibilityName, "ALWAYS");
	ASSERT_EQ(geom1.vertexGroups[0].vertexes.size(), 3u);
	EXPECT_DOUBLE_EQ(geom1.vertexGroups[0].vertexes[0].posn[0], 0.0);
	EXPECT_DOUBLE_EQ(geom1.vertexGroups[0].vertexes[1].posn[0], 1.0);

	HiveNodeDescriptor geomScript1 = loader.getNode(5);
	EXPECT_EQ(geomScript1.type, HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT);
	EXPECT_EQ(geomScript1.coreScript, "");
	EXPECT_EQ(geomScript1.pokeScript, "");
	ASSERT_EQ(geomScript1.vertexGroups.size(), 1u);
	ASSERT_EQ(geomScript1.vertexGroups[0].vertexes.size(), 1u);
	EXPECT_EQ(std::to_integer<int>(geomScript1.vertexGroups[0].vertexes[0].colour[0]), 255);

	HiveNodeDescriptor xform1 = loader.getNode(6);
	EXPECT_EQ(xform1.type, HiveNodeDescriptor::SCENE_TRANSFORM);
	EXPECT_TRUE(xform1.hasTransform);
	EXPECT_DOUBLE_EQ(xform1.transform[0], 1.0);
	EXPECT_DOUBLE_EQ(xform1.transform[5], 1.0);

	HiveNodeDescriptor xformScript1 = loader.getNode(7);
	EXPECT_EQ(xformScript1.type, HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT);
	EXPECT_FALSE(xformScript1.hasTransform);
	EXPECT_EQ(xformScript1.coreScript, "");

	HiveNodeDescriptor agent1 = loader.getNode(8);
	EXPECT_EQ(agent1.type, HiveNodeDescriptor::AGENT);
	EXPECT_EQ(agent1.capabilityName, "HIGH");
	ASSERT_EQ(agent1.prompts.size(), 1u);
	EXPECT_EQ(agent1.prompts[0].nodeTypeName, "PING_NODE");
	EXPECT_EQ(agent1.prompts[0].prompt, "describe this node");
	EXPECT_EQ(agent1.prompts[0].nodeIdentifier, "");
	EXPECT_FALSE(agent1.prompts[0].terminateOnResponse);

	ASSERT_EQ(loader.getStrobeEmitterCount(), 1u);

	std::string strobeNodeName;
	unsigned strobePeriodMs;
	loader.getStrobeEmitter(0, strobeNodeName, strobePeriodMs);

	EXPECT_EQ(strobeNodeName, "root1");
	EXPECT_EQ(strobePeriodMs, 100u);
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
 * An edge's "traversalKey" defaults to 0 when omitted and is carried through when supplied.
 */
TEST(JsonHiveLoaderTest, EdgeTraversalKey_IsParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "PingNode", "name": "a",
				"edges": [ { "toNodeName": "b" }, { "toNodeName": "b", "traversalKey": 4294967295 } ] },
			{ "type": "PingNode", "name": "b" }
		]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor a = loader.getNode(0);

	ASSERT_EQ(a.edges.size(), 2u);
	EXPECT_EQ(a.edges[0].traversalKey, 0u);
	EXPECT_EQ(a.edges[1].traversalKey, 4294967295u);
}

/**
 * An edge's "traversalKey" must be a non-negative integer that fits in 32 bits.
 */
TEST(JsonHiveLoaderTest, EdgeInvalidTraversalKey_ThrowsJsonInvalidEdges)
{
	const char* badKeys[] = { "\"7\"", "-1", "1.5", "4294967296", "true" };

	for(const char* badKey : badKeys)
	{
		std::string json = std::string(R"({ "name": "Hive", "nodes": [
			{ "type": "PingNode", "name": "a", "edges": [ { "toNodeName": "b", "traversalKey": )") + badKey + R"( } ] },
			{ "type": "PingNode", "name": "b" } ] })";

		EXPECT_THROW(JsonHiveLoader loader(json), PersistException) << "Accepted traversalKey " << badKey;
	}
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
 * Visibility belongs to a group of vertexes rather than to a vertex, so each run of consecutive vertexes
 * sharing one visibility becomes a group, in the order the vertexes were written.
 */
TEST(JsonHiveLoaderTest, VertexVisibilityRuns_AreGroupedInOrder)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "SceneGeometryNode", "name": "g1", "vertexes": [
				{ "posn": [0, 0, 0] },
				{ "posn": [1, 0, 0], "visibility": "HOVERED_OVER" },
				{ "posn": [2, 0, 0], "visibility": "HOVERED_OVER" },
				{ "posn": [3, 0, 0], "visibility": "ALWAYS" }
			] }
		]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor geom1 = loader.getNode(0);

	ASSERT_EQ(geom1.vertexGroups.size(), 3u);

	EXPECT_EQ(geom1.vertexGroups[0].visibilityName, "ALWAYS")
		<< "A vertex written without a visibility should take the schema default.";
	ASSERT_EQ(geom1.vertexGroups[0].vertexes.size(), 1u);

	EXPECT_EQ(geom1.vertexGroups[1].visibilityName, "HOVERED_OVER");
	ASSERT_EQ(geom1.vertexGroups[1].vertexes.size(), 2u)
		<< "Consecutive vertexes sharing a visibility should share a group.";
	EXPECT_DOUBLE_EQ(geom1.vertexGroups[1].vertexes[0].posn[0], 1.0);
	EXPECT_DOUBLE_EQ(geom1.vertexGroups[1].vertexes[1].posn[0], 2.0);

	EXPECT_EQ(geom1.vertexGroups[2].visibilityName, "ALWAYS")
		<< "Returning to a visibility already used should open a new group, not rejoin the earlier one.";
	ASSERT_EQ(geom1.vertexGroups[2].vertexes.size(), 1u);
}

/**
 * The grouped form states a visibility once for a whole group, and arrives as the same descriptor the flat
 * form builds a run into.
 */
TEST(JsonHiveLoaderTest, VertexGroups_StateVisibilityOncePerGroup)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "SceneGeometryNode", "name": "g1", "vertexGroups": [
				{ "vertexes": [ { "posn": [0, 0, 0] } ] },
				{ "visibility": "HOVERED_OVER", "vertexes": [ { "posn": [1, 0, 0] }, { "posn": [2, 0, 0] } ] }
			] }
		]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor geom1 = loader.getNode(0);

	ASSERT_EQ(geom1.vertexGroups.size(), 2u);

	EXPECT_EQ(geom1.vertexGroups[0].visibilityName, "ALWAYS")
		<< "A group written without a visibility should take the schema default.";
	ASSERT_EQ(geom1.vertexGroups[0].vertexes.size(), 1u);

	EXPECT_EQ(geom1.vertexGroups[1].visibilityName, "HOVERED_OVER");
	ASSERT_EQ(geom1.vertexGroups[1].vertexes.size(), 2u);
	EXPECT_DOUBLE_EQ(geom1.vertexGroups[1].vertexes[0].posn[0], 1.0);
	EXPECT_DOUBLE_EQ(geom1.vertexGroups[1].vertexes[1].posn[0], 2.0);
}

/**
 * The two forms are alternative spellings of one thing, so a node holding both is rejected rather than left
 * saying nothing about the order they would combine in.
 */
TEST(JsonHiveLoaderTest, BothVertexesAndVertexGroups_ThrowsJsonVertexesAndVertexGroups)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "SceneGeometryNode", "name": "g1",
				"vertexes": [ { "posn": [0, 0, 0] } ],
				"vertexGroups": [ { "vertexes": [ { "posn": [1, 0, 0] } ] } ] }
		]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A vertex group missing its required "vertexes" is rejected.
 */
TEST(JsonHiveLoaderTest, VertexGroupMissingVertexes_ThrowsJsonInvalidVertexGroups)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "SceneGeometryNode", "name": "g1", "vertexGroups": [ { "visibility": "AGENT" } ] }
		]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A group holding no vertexes would only ever reach a surface as an empty chunk, so it is dropped rather
 * than carried through the build.
 */
TEST(JsonHiveLoaderTest, EmptyVertexGroup_IsDropped)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "SceneGeometryNode", "name": "g1", "vertexGroups": [
				{ "visibility": "AGENT", "vertexes": [] },
				{ "vertexes": [ { "posn": [0, 0, 0] } ] }
			] }
		]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor geom1 = loader.getNode(0);

	ASSERT_EQ(geom1.vertexGroups.size(), 1u);
	EXPECT_EQ(geom1.vertexGroups[0].visibilityName, "ALWAYS");
}

/**
 * A vertex "visibility" that is present but not a string is rejected.
 */
TEST(JsonHiveLoaderTest, VertexNonStringVisibility_ThrowsJsonInvalidVertexes)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "SceneGeometryNode", "name": "g1", "vertexes": [ { "posn": [0, 0, 0], "visibility": 3 } ] }
		]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A vertex "visibility" naming a visibility that doesn't exist is a string as far as the loader is
 * concerned, so it is the builder that rejects it.
 */
TEST(JsonHiveLoaderTest, VertexUnknownVisibility_ThrowsUnknownVertexVisibilityWhenBuilt)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "SceneGeometryNode", "name": "g1",
				"vertexes": [ { "posn": [0, 0, 0], "visibility": "NOT_A_VISIBILITY" } ] }
		]
	})";

	JsonHiveLoader loader(json);

	EXPECT_THROW(HiveBuilder::build(loader, 2), PersistException);
}

/**
 * A SceneGeometryNode omitting "emitAgentAffectAction" defaults it to false, per the schema.
 */
TEST(JsonHiveLoaderTest, SceneGeometryNodeOmittingEmitAgentAffectAction_DefaultsToFalse)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneGeometryNode", "name": "g1" } ]
	})";

	JsonHiveLoader loader(json);

	EXPECT_FALSE(loader.getNode(0).emitAgentAffectAction);
}

/**
 * A SceneGeometryNode's "emitAgentAffectAction" is carried through when supplied.
 */
TEST(JsonHiveLoaderTest, SceneGeometryNodeEmitAgentAffectActionTrue_IsParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneGeometryNode", "name": "g1", "emitAgentAffectAction": true } ]
	})";

	JsonHiveLoader loader(json);

	EXPECT_TRUE(loader.getNode(0).emitAgentAffectAction);
}

/**
 * A SceneGeometryNode's "emitAgentAffectAction" member must be a boolean if present.
 */
TEST(JsonHiveLoaderTest, SceneGeometryNodeNonBooleanEmitAgentAffectAction_ThrowsJsonInvalidEmitAgentAffectAction)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneGeometryNode", "name": "g1", "emitAgentAffectAction": "yes" } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A SceneTransformNode omitting "emitAgentAffectAction" defaults it to false, per the schema.
 */
TEST(JsonHiveLoaderTest, SceneTransformNodeOmittingEmitAgentAffectAction_DefaultsToFalse)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTransformNode", "name": "x1" } ]
	})";

	JsonHiveLoader loader(json);

	EXPECT_FALSE(loader.getNode(0).emitAgentAffectAction);
}

/**
 * A SceneTransformNode's "emitAgentAffectAction" is carried through when supplied.
 */
TEST(JsonHiveLoaderTest, SceneTransformNodeEmitAgentAffectActionTrue_IsParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTransformNode", "name": "x1", "emitAgentAffectAction": true } ]
	})";

	JsonHiveLoader loader(json);

	EXPECT_TRUE(loader.getNode(0).emitAgentAffectAction);
}

/**
 * A SceneTransformNode's "emitAgentAffectAction" member must be a boolean if present.
 */
TEST(JsonHiveLoaderTest, SceneTransformNodeNonBooleanEmitAgentAffectAction_ThrowsJsonInvalidEmitAgentAffectAction)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTransformNode", "name": "x1", "emitAgentAffectAction": "yes" } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A SceneTransformScriptNode's "emitAgentAffectAction" is carried through when supplied.
 */
TEST(JsonHiveLoaderTest, SceneTransformScriptNodeEmitAgentAffectActionTrue_IsParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{
				"type": "SceneTransformScriptNode",
				"name": "x1",
				"coreScript": "",
				"pokeScript": "",
				"emitAgentAffectAction": true
			}
		]
	})";

	JsonHiveLoader loader(json);

	EXPECT_TRUE(loader.getNode(0).emitAgentAffectAction);
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
 * An AgentNode omitting both of its optional booleans defaults them to true, per the schema, and
 * parses a prompt that carries an explicit node identifier.
 */
TEST(JsonHiveLoaderTest, AgentNodeOmittingOptionalFlags_DefaultsBothToTrue)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "AgentNode", "name": "a1", "capability": "LOW",
				"prompts": [ { "nodeIdentifier": "geom1", "nodeType": "SCENE_GEOMETRY_NODE", "prompt": "summarise" } ] }
		]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor descriptor = loader.getNode(0);

	EXPECT_TRUE(descriptor.autoTriggerAgentAction);
	EXPECT_TRUE(descriptor.serialiseEmittedActions);
	ASSERT_EQ(descriptor.prompts.size(), 1u);
	EXPECT_EQ(descriptor.prompts[0].nodeIdentifier, "geom1");
}

/**
 * An AgentNode's optional booleans are carried through when supplied.
 */
TEST(JsonHiveLoaderTest, AgentNodeOptionalFlagsFalse_AreParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "AgentNode", "name": "a1", "capability": "MEDIUM",
				"autoTriggerAgentAction": false, "serialiseEmittedActions": false,
				"prompts": [ { "nodeType": "PING_NODE", "prompt": "p" } ] }
		]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor descriptor = loader.getNode(0);

	EXPECT_FALSE(descriptor.autoTriggerAgentAction);
	EXPECT_FALSE(descriptor.serialiseEmittedActions);
}

/**
 * An AgentNode prompt's "terminateOnResponse" is carried through when supplied.
 */
TEST(JsonHiveLoaderTest, AgentNodePromptTerminateOnResponseTrue_IsParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "AgentNode", "name": "a1", "capability": "MEDIUM",
				"prompts": [ { "nodeType": "PING_NODE", "prompt": "p", "terminateOnResponse": true } ] }
		]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor descriptor = loader.getNode(0);

	ASSERT_EQ(descriptor.prompts.size(), 1u);
	EXPECT_TRUE(descriptor.prompts[0].terminateOnResponse);
}

/**
 * An AgentNode prompt's "terminateOnResponse" must be a boolean if present.
 */
TEST(JsonHiveLoaderTest, AgentNodePromptNonBooleanTerminateOnResponse_ThrowsJsonInvalidAgentPrompts)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "AgentNode", "name": "a1", "capability": "LOW",
				"prompts": [ { "nodeType": "PING_NODE", "prompt": "p", "terminateOnResponse": "yes" } ] }
		]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * An AgentNode missing its required "capability" is rejected.
 */
TEST(JsonHiveLoaderTest, AgentNodeMissingCapability_ThrowsJsonInvalidAgentCapability)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "AgentNode", "name": "a1", "prompts": [ { "nodeType": "PING_NODE", "prompt": "p" } ] }
		]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * An AgentNode with an empty "prompts" array is rejected; it would have nothing to emit.
 */
TEST(JsonHiveLoaderTest, AgentNodeEmptyPrompts_ThrowsJsonInvalidAgentPrompts)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "AgentNode", "name": "a1", "capability": "LOW", "prompts": [] } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * An AgentNode prompt missing its required "prompt" is rejected.
 */
TEST(JsonHiveLoaderTest, AgentNodePromptMissingPromptText_ThrowsJsonInvalidAgentPrompts)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "AgentNode", "name": "a1", "capability": "LOW",
				"prompts": [ { "nodeType": "PING_NODE" } ] }
		]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * An AgentNode's "autoTriggerAgentAction" member must be a boolean if present.
 */
TEST(JsonHiveLoaderTest, AgentNodeNonBooleanAutoTrigger_ThrowsJsonInvalidAgentAutoTrigger)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "AgentNode", "name": "a1", "capability": "LOW", "autoTriggerAgentAction": "yes",
				"prompts": [ { "nodeType": "PING_NODE", "prompt": "p" } ] }
		]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A TriggerNode is a script type, so its scripts are parsed like any other script node's, and
 * "emitTriggerOnPoke" defaults to true when omitted, per the schema.
 */
TEST(JsonHiveLoaderTest, TriggerNodeOmittingEmitOnPoke_ParsesScriptsAndDefaultsToTrue)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "TriggerNode", "name": "t1", "coreScript": "core", "pokeScript": "poke" }
		]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor descriptor = loader.getNode(0);

	EXPECT_EQ(descriptor.type, HiveNodeDescriptor::TRIGGER);
	EXPECT_EQ(descriptor.coreScript, "core");
	EXPECT_EQ(descriptor.pokeScript, "poke");
	EXPECT_TRUE(descriptor.emitTriggerOnPoke);
}

/**
 * A TriggerNode's "emitTriggerOnPoke" is carried through when supplied.
 */
TEST(JsonHiveLoaderTest, TriggerNodeEmitOnPokeFalse_IsParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "TriggerNode", "name": "t1", "coreScript": "", "pokeScript": "",
				"emitTriggerOnPoke": false }
		]
	})";

	JsonHiveLoader loader(json);

	EXPECT_FALSE(loader.getNode(0).emitTriggerOnPoke);
}

/**
 * A TriggerNode missing its required "coreScript" is rejected, the same as any other script node.
 */
TEST(JsonHiveLoaderTest, TriggerNodeMissingCoreScript_ThrowsJsonInvalidScriptSource)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "TriggerNode", "name": "t1", "pokeScript": "" } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A TriggerNode's "emitTriggerOnPoke" member must be a boolean if present.
 */
TEST(JsonHiveLoaderTest, TriggerNodeNonBooleanEmitOnPoke_ThrowsJsonInvalidTriggerEmitOnPoke)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "TriggerNode", "name": "t1", "coreScript": "", "pokeScript": "",
				"emitTriggerOnPoke": "yes" }
		]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A SceneTextScriptNode's text fields are all carried through when supplied, alongside its scripts.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeFields_AreParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [
			{ "type": "SceneTextScriptNode", "name": "label", "coreScript": "core", "pokeScript": "poke",
				"text": "Hello\nworld", "textPosition": [1, 2, 3], "textHeight": 0.5,
				"textColour": [10, 20, 30, 40] }
		]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor descriptor = loader.getNode(0);

	EXPECT_EQ(descriptor.type, HiveNodeDescriptor::SCENE_TEXT_SCRIPT);
	EXPECT_EQ(descriptor.coreScript, "core");
	EXPECT_EQ(descriptor.pokeScript, "poke");
	EXPECT_EQ(descriptor.text, "Hello\nworld");
	EXPECT_DOUBLE_EQ(descriptor.textPosition[0], 1);
	EXPECT_DOUBLE_EQ(descriptor.textPosition[1], 2);
	EXPECT_DOUBLE_EQ(descriptor.textPosition[2], 3);
	EXPECT_DOUBLE_EQ(descriptor.textHeight, 0.5);
	EXPECT_EQ(std::to_integer<int>(descriptor.textColour[0]), 10);
	EXPECT_EQ(std::to_integer<int>(descriptor.textColour[3]), 40);
}

/**
 * A SceneTextScriptNode omitting its text fields gets empty white text one unit high at its origin.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeOmittingFields_UsesDefaults)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "" } ]
	})";

	JsonHiveLoader loader(json);

	HiveNodeDescriptor descriptor = loader.getNode(0);

	EXPECT_EQ(descriptor.text, "");
	EXPECT_DOUBLE_EQ(descriptor.textPosition[0], 0);
	EXPECT_DOUBLE_EQ(descriptor.textHeight, 1.0);
	EXPECT_EQ(std::to_integer<int>(descriptor.textColour[0]), 255);
	EXPECT_TRUE(descriptor.textVisible);
}

/**
 * A SceneTextScriptNode's "textBackground" is parsed, with a radius per corner.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeTextBackground_IsParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
			"textBackground": { "fillColour": [1, 2, 3, 4], "borderColour": [5, 6, 7, 8], "borderWidth": 0.05,
				"padding": 0.2, "cornerRadius": [0.1, 0.2, 0.3, 0.4] } } ]
	})";

	JsonHiveLoader loader(json);

	const TextBackground& background = loader.getNode(0).textBackground;

	EXPECT_EQ(std::to_integer<int>(background.fillColour[0]), 1);
	EXPECT_EQ(std::to_integer<int>(background.fillColour[3]), 4);
	EXPECT_EQ(std::to_integer<int>(background.borderColour[0]), 5);
	EXPECT_EQ(std::to_integer<int>(background.borderColour[3]), 8);
	EXPECT_DOUBLE_EQ(background.borderWidth, 0.05);
	EXPECT_DOUBLE_EQ(background.padding, 0.2);
	EXPECT_DOUBLE_EQ(background.cornerRadii[TextBackground::TOP_LEFT], 0.1);
	EXPECT_DOUBLE_EQ(background.cornerRadii[TextBackground::BOTTOM_LEFT], 0.4);
}

/**
 * A single "cornerRadius" rounds every corner alike, and what is left out keeps the default.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeTextBackgroundSingleRadius_RoundsEveryCorner)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
			"textBackground": { "cornerRadius": 0.3 } } ]
	})";

	JsonHiveLoader loader(json);

	const TextBackground& background = loader.getNode(0).textBackground;

	for(int corner = 0; corner < TextBackground::CORNER_COUNT; corner++)
	{
		EXPECT_DOUBLE_EQ(background.cornerRadii[corner], 0.3);
	}

	EXPECT_EQ(std::to_integer<int>(background.fillColour[3]), 0);
	EXPECT_DOUBLE_EQ(background.borderWidth, 0.0);
}

/**
 * A malformed or out of range "textBackground" is refused.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeInvalidTextBackground_ThrowsJsonInvalidTextBackground)
{
	const char* backgrounds[] =
	{
		"[]",
		"{ \"fillColour\": [0, 0, 0] }",
		"{ \"borderColour\": [0, 0, 0, 256] }",
		"{ \"borderWidth\": -1 }",
		"{ \"padding\": \"wide\" }",
		"{ \"cornerRadius\": [1, 1, 1] }",
		"{ \"cornerRadius\": -0.5 }"
	};

	for(const char* background : backgrounds)
	{
		std::string json = std::string(R"({
			"name": "Hive",
			"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
				"textBackground": )") + background + " } ] }";

		try
		{
			JsonHiveLoader loader(json);
			ADD_FAILURE() << "Expected PersistException for " << background;
		}
		catch(PersistException& exception)
		{
			EXPECT_EQ(exception.getError(), PersistException::JSON_INVALID_TEXT_BACKGROUND) << background;
		}
	}
}

/**
 * A SceneTextScriptNode can be loaded with its text hidden.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeTextVisible_IsParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
			"textVisible": false } ]
	})";

	JsonHiveLoader loader(json);

	EXPECT_FALSE(loader.getNode(0).textVisible);
}

/**
 * A SceneTextScriptNode's "textVisible" must be a boolean.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeNonBooleanTextVisible_ThrowsJsonInvalidTextVisible)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
			"textVisible": 0 } ]
	})";

	try
	{
		JsonHiveLoader loader(json);
		FAIL() << "Expected PersistException";
	}
	catch(PersistException& exception)
	{
		EXPECT_EQ(exception.getError(), PersistException::JSON_INVALID_TEXT_VISIBLE);
	}
}

/**
 * A SceneTextScriptNode's "textHeight" must be greater than zero.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeNonPositiveHeight_ThrowsJsonInvalidTextHeight)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
			"textHeight": 0 } ]
	})";

	try
	{
		JsonHiveLoader loader(json);
		FAIL() << "Expected PersistException";
	}
	catch(PersistException& exception)
	{
		EXPECT_EQ(exception.getError(), PersistException::JSON_INVALID_TEXT_HEIGHT);
	}
}

/**
 * A SceneTextScriptNode's "textColour" channels must each be an integer from 0 to 255.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeOutOfRangeColour_ThrowsJsonInvalidTextColour)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
			"textColour": [256, 0, 0, 255] } ]
	})";

	try
	{
		JsonHiveLoader loader(json);
		FAIL() << "Expected PersistException";
	}
	catch(PersistException& exception)
	{
		EXPECT_EQ(exception.getError(), PersistException::JSON_INVALID_TEXT_COLOUR);
	}
}

/**
 * A SceneTextScriptNode's "text" must be a string, and its "textPosition" three numbers.
 */
TEST(JsonHiveLoaderTest, SceneTextScriptNodeMalformedTextOrPosition_Throws)
{
	std::string badText = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
			"text": 5 } ]
	})";

	std::string badPosition = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
			"textPosition": [1, 2] } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(badText), PersistException);
	EXPECT_THROW(JsonHiveLoader loader(badPosition), PersistException);
}

/**
 * End-to-end: a SceneTextScriptNode built from JSON carries the text fields it was loaded with.
 */
TEST(JsonHiveLoaderTest, EndToEnd_SceneTextScriptNodeBuiltWithFields)
{
	std::string json = R"({
		"name": "TestHive",
		"nodes": [
			{ "type": "SceneTextScriptNode", "name": "label", "coreScript": "", "pokeScript": "",
				"text": "Sign", "textPosition": [4, 5, 6], "textHeight": 2, "textColour": [1, 2, 3, 4],
				"textVisible": false }
		]
	})";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> labelHandle = hive -> getNode("label");

	ASSERT_TRUE(labelHandle.isValid());
	ASSERT_EQ(labelHandle.getInstance() -> getType(), GraphNodeType::SCENE_TEXT_SCRIPT_NODE);

	SceneTextScriptNode* labelNode = dynamic_cast<SceneTextScriptNode*>(labelHandle.getInstance());

	ASSERT_NE(labelNode, nullptr);

	double position[3];
	std::byte colour[4];

	labelNode -> getPosition(position);
	labelNode -> getColour(colour);

	EXPECT_EQ(labelNode -> getText(), "Sign");
	EXPECT_DOUBLE_EQ(position[0], 4);
	EXPECT_DOUBLE_EQ(position[1], 5);
	EXPECT_DOUBLE_EQ(position[2], 6);
	EXPECT_DOUBLE_EQ(labelNode -> getHeight(), 2);
	EXPECT_EQ(std::to_integer<int>(colour[0]), 1);
	EXPECT_EQ(std::to_integer<int>(colour[3]), 4);
	EXPECT_FALSE(labelNode -> getVisible());

	hive -> shutdown();
}

/**
 * A strobeEmitters entry omitting "periodMs" defaults to 33 ms.
 */
TEST(JsonHiveLoaderTest, StrobeEmitterMissingPeriodMs_DefaultsTo33)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"strobeEmitters": [ { "nodeName": "root1" } ]
	})";

	JsonHiveLoader loader(json);

	ASSERT_EQ(loader.getStrobeEmitterCount(), 1u);

	std::string strobeNodeName;
	unsigned strobePeriodMs;
	loader.getStrobeEmitter(0, strobeNodeName, strobePeriodMs);

	EXPECT_EQ(strobeNodeName, "root1");
	EXPECT_EQ(strobePeriodMs, 33u);
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
		"strobeSurfaces": [ { "surfaceName": "surface1", "periodMs": 100 } ]
	})";

	JsonHiveLoader loader(json);

	ASSERT_EQ(loader.getSurfaceCount(), 1u);

	HiveSurfaceDescriptor surface1 = loader.getSurface(0);
	EXPECT_EQ(surface1.type, HiveSurfaceDescriptor::SCENE_SURFACE);
	EXPECT_EQ(surface1.name, "surface1");
	EXPECT_EQ(surface1.sceneRootNodeName, "root1");

	ASSERT_EQ(loader.getStrobeSurfaceCount(), 1u);

	std::string strobeSurfaceName;
	unsigned strobePeriodMs;
	loader.getStrobeSurface(0, strobeSurfaceName, strobePeriodMs);

	EXPECT_EQ(strobeSurfaceName, "surface1");
	EXPECT_EQ(strobePeriodMs, 100u);
}

/**
 * A GraphHiveGraphViewSurface parses without naming a node to be bound to, since it presents the hive
 * itself rather than anything within it.
 */
TEST(JsonHiveLoaderTest, GraphViewSurfaceParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "PingNode", "name": "a" } ],
		"surfaces": [ { "type": "GraphHiveGraphViewSurface", "name": "structure", "default": true } ],
		"strobeSurfaces": [ { "surfaceName": "structure", "periodMs": 250 } ]
	})";

	JsonHiveLoader loader(json);

	ASSERT_EQ(loader.getSurfaceCount(), 1u);

	HiveSurfaceDescriptor surface1 = loader.getSurface(0);
	EXPECT_EQ(surface1.type, HiveSurfaceDescriptor::GRAPH_VIEW_SURFACE);
	EXPECT_EQ(surface1.name, "structure");
	EXPECT_TRUE(surface1.isDefault);
	EXPECT_EQ(surface1.sceneRootNodeName, "");

	ASSERT_EQ(loader.getStrobeSurfaceCount(), 1u);

	std::string strobeSurfaceName;
	unsigned strobePeriodMs;
	loader.getStrobeSurface(0, strobeSurfaceName, strobePeriodMs);

	EXPECT_EQ(strobeSurfaceName, "structure");
	EXPECT_EQ(strobePeriodMs, 250u);
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
 * A "focusViewportFraction" of a whole viewport is the largest one accepted.
 */
TEST(JsonHiveLoaderTest, SurfaceFocusViewportFractionOfWholeViewport_IsParsed)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "type": "GraphHiveSceneSurface", "name": "surface1", "sceneRootNodeName": "root1",
			"initialFocusNodeName": "root1", "focusViewportFraction": 1.0 } ]
	})";

	JsonHiveLoader loader(json);

	ASSERT_EQ(loader.getSurfaceCount(), 1u);

	EXPECT_EQ(loader.getSurface(0).focusViewportFraction, 1.0);
}

/**
 * A "focusViewportFraction" above a whole viewport is out of range, so it is rejected rather than left to set
 * up a camera that cannot frame what it was pointed at.
 */
TEST(JsonHiveLoaderTest, SurfaceFocusViewportFractionAboveOne_ThrowsJsonInvalidSurfaceFocusViewportFraction)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "type": "GraphHiveSceneSurface", "name": "surface1", "sceneRootNodeName": "root1",
			"initialFocusNodeName": "root1", "focusViewportFraction": 1.5 } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A "focusViewportFraction" of zero or less leaves the focused node no room at all, so it is rejected.
 */
TEST(JsonHiveLoaderTest, SurfaceFocusViewportFractionOfZero_ThrowsJsonInvalidSurfaceFocusViewportFraction)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "type": "GraphHiveSceneSurface", "name": "surface1", "sceneRootNodeName": "root1",
			"initialFocusNodeName": "root1", "focusViewportFraction": 0 } ]
	})";

	EXPECT_THROW(JsonHiveLoader loader(json), PersistException);
}

/**
 * A strobeSurfaces entry omitting "periodMs" defaults to 33 ms.
 */
TEST(JsonHiveLoaderTest, StrobeSurfaceMissingPeriodMs_DefaultsTo33)
{
	std::string json = R"({
		"name": "Hive",
		"nodes": [ { "type": "SceneRootNode", "name": "root1" } ],
		"surfaces": [ { "type": "GraphHiveSceneSurface", "name": "surface1", "sceneRootNodeName": "root1" } ],
		"strobeSurfaces": [ { "surfaceName": "surface1" } ]
	})";

	JsonHiveLoader loader(json);

	ASSERT_EQ(loader.getStrobeSurfaceCount(), 1u);

	std::string strobeSurfaceName;
	unsigned strobePeriodMs;
	loader.getStrobeSurface(0, strobeSurfaceName, strobePeriodMs);

	EXPECT_EQ(strobeSurfaceName, "surface1");
	EXPECT_EQ(strobePeriodMs, 33u);
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
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> sourceHandle = hive -> getNode("source");
	Handle<GraphNode> targetHandle = hive -> getNode("target");

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
 * End-to-end: an edge with "actionsCompleteAfterTraverse" set is still applied to as normal, but the
 * action completes there rather than continuing on to a further node.
 */
TEST(JsonHiveLoaderTest, EndToEnd_ActionsCompleteAfterTraverseEdgeStopsTraversal)
{
	std::string json = R"({
		"name": "TestHive",
		"nodes": [
			{ "type": "PingNode", "name": "source",
				"edges": [ { "toNodeName": "mid", "actionsCompleteAfterTraverse": true } ] },
			{ "type": "PingNode", "name": "mid", "edges": [ { "toNodeName": "target" } ] },
			{ "type": "PingNode", "name": "target" }
		]
	})";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> sourceHandle = hive -> getNode("source");
	Handle<GraphNode> midHandle = hive -> getNode("mid");
	Handle<GraphNode> targetHandle = hive -> getNode("target");

	ASSERT_TRUE(sourceHandle.isValid());
	ASSERT_TRUE(midHandle.isValid());
	ASSERT_TRUE(targetHandle.isValid());

	PingNode* sourceNode = dynamic_cast<PingNode*>(sourceHandle.getInstance());
	PingNode* midNode = dynamic_cast<PingNode*>(midHandle.getInstance());
	PingNode* targetNode = dynamic_cast<PingNode*>(targetHandle.getInstance());

	ASSERT_NE(sourceNode, nullptr);
	ASSERT_NE(midNode, nullptr);
	ASSERT_NE(targetNode, nullptr);

	PingAction* action = sourceNode -> emitPing(true);

	// Applied normally to the node at the end of the flagged edge...
	EXPECT_EQ(midNode -> getPingCount(), 1u);
	// ...but never carries on to traverse past it.
	EXPECT_EQ(targetNode -> getPingCount(), 0u);

	action -> decrRef();

	hive -> shutdown();
}

/**
 * End-to-end: a keyed edge built from JSON carries its key, and a ping running on the hive's worker threads
 * (where the key check runs under the action's lock) skips it for the unkeyed sibling without deadlocking.
 */
TEST(JsonHiveLoaderTest, EndToEnd_KeyedEdgeSkippedByActionWithoutKey)
{
	std::string json = R"({
		"name": "TestHive",
		"nodes": [
			{ "type": "PingNode", "name": "source",
				"edges": [
					{ "toNodeName": "locked", "actionFlags": ["PING_GRAPH_ACTION"], "traversalKey": 9 },
					{ "toNodeName": "open" }
				] },
			{ "type": "PingNode", "name": "locked" },
			{ "type": "PingNode", "name": "open" }
		]
	})";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> sourceHandle = hive -> getNode("source");
	Handle<GraphNode> lockedHandle = hive -> getNode("locked");
	Handle<GraphNode> openHandle = hive -> getNode("open");

	ASSERT_TRUE(sourceHandle.isValid());
	ASSERT_TRUE(lockedHandle.isValid());
	ASSERT_TRUE(openHandle.isValid());

	std::vector<Handle<GraphEdge>> edges = sourceHandle.getInstance() -> getEdges();

	ASSERT_EQ(edges.size(), 2u);
	EXPECT_EQ(edges[0].getInstance() -> getTraversalKey(), 9u);
	EXPECT_EQ(edges[1].getInstance() -> getTraversalKey(), 0u);

	PingNode* sourceNode = dynamic_cast<PingNode*>(sourceHandle.getInstance());
	PingNode* lockedNode = dynamic_cast<PingNode*>(lockedHandle.getInstance());
	PingNode* openNode = dynamic_cast<PingNode*>(openHandle.getInstance());

	ASSERT_NE(sourceNode, nullptr);
	ASSERT_NE(lockedNode, nullptr);
	ASSERT_NE(openNode, nullptr);

	PingAction* action = sourceNode -> emitPing(true);

	EXPECT_EQ(lockedNode -> getPingCount(), 0u);
	EXPECT_EQ(openNode -> getPingCount(), 1u);

	action -> decrRef();

	hive -> shutdown();
}

/**
 * End-to-end: the visibility written against JSON vertexes survives the build and reaches a scene
 * surface as separate chunks, exactly as the same geometry added from Lua would.
 */
TEST(JsonHiveLoaderTest, EndToEnd_JsonVertexVisibilityReachesSurfaceAsChunks)
{
	std::string json = R"({
		"name": "TestHive",
		"nodes": [
			{ "type": "SceneRootNode", "name": "root1", "edges": [ { "toNodeName": "g1" } ] },
			{ "type": "SceneGeometryNode", "name": "g1", "vertexes": [
				{ "posn": [1, 0, 0] },
				{ "posn": [0, 1, 0], "visibility": "AGENT" }
			] }
		]
	})";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> rootHandle = hive -> getNode("root1");

	ASSERT_TRUE(rootHandle.isValid());

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(0));

	surface -> setHive(hiveHandle);

	SceneAction* sceneAction = new SceneAction(rootHandle, Handle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;

	ASSERT_EQ(chunks.size(), 2u) << "Two runs of differing visibility should arrive as two chunks.";

	ASSERT_EQ(chunks[0].vertexes.size(), 1u);
	ASSERT_EQ(chunks[1].vertexes.size(), 1u);

	EXPECT_EQ(chunks[0].visibility, SceneGeometry::VertexVisibility::ALWAYS)
		<< "A vertex written without a visibility should default to ALWAYS.";
	EXPECT_EQ(chunks[1].visibility, SceneGeometry::VertexVisibility::AGENT)
		<< "A vertex written with \"AGENT\" should land in an AGENT chunk.";

	sceneAction -> decrRef();

	surface -> close();

	hive -> shutdown();
}

/**
 * End-to-end: geometry written as vertex groups reaches a surface as the same chunks the flat form
 * produces, so the two forms are interchangeable all the way through the build.
 */
TEST(JsonHiveLoaderTest, EndToEnd_JsonVertexGroupsReachSurfaceAsChunks)
{
	std::string json = R"({
		"name": "TestHive",
		"nodes": [
			{ "type": "SceneRootNode", "name": "root1", "edges": [ { "toNodeName": "g1" } ] },
			{ "type": "SceneGeometryNode", "name": "g1", "vertexGroups": [
				{ "vertexes": [ { "posn": [1, 0, 0] } ] },
				{ "visibility": "AGENT", "vertexes": [ { "posn": [0, 1, 0] } ] }
			] }
		]
	})";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> rootHandle = hive -> getNode("root1");

	ASSERT_TRUE(rootHandle.isValid());

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(0));

	surface -> setHive(hiveHandle);

	SceneAction* sceneAction = new SceneAction(rootHandle, Handle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;

	ASSERT_EQ(chunks.size(), 2u);
	ASSERT_EQ(chunks[0].vertexes.size(), 1u);
	ASSERT_EQ(chunks[1].vertexes.size(), 1u);

	EXPECT_EQ(chunks[0].visibility, SceneGeometry::VertexVisibility::ALWAYS);
	EXPECT_EQ(chunks[1].visibility, SceneGeometry::VertexVisibility::AGENT);

	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[0], 1.0)
		<< "Groups should reach the surface in the order they were written.";

	sceneAction -> decrRef();

	surface -> close();

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
		"strobeSurfaces": [ { "surfaceName": "surface1", "periodMs": 100 } ]
	})";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	EXPECT_TRUE(hive -> getSurface("surface1").isValid());

	hive -> shutdown();
}

#endif
