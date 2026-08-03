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
