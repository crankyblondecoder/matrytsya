#ifndef HIVE_PERSIST_UNIT_TEST_H
#define HIVE_PERSIST_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../graph/GraphEdge.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphNode.hpp"
#include "../../graph/graphActionFlagRegister.hpp"
#include "../../graph/nodes/BuilderScriptNode.hpp"
#include "../../graph/nodes/SceneTransformNode.hpp"
#include "../../graph/nodes/ScriptSession.hpp"
#include "../../persist/HiveBuilder.hpp"
#include "../../persist/HiveNodeDescriptor.hpp"
#include "../../persist/HivePersister.hpp"
#include "../../persist/HiveSurfaceDescriptor.hpp"
#include "../../persist/PersistException.hpp"
#include "../../persist/json/JsonHiveLoader.hpp"
#include "../../persist/json/JsonHiveWriter.hpp"
#include "../../persist/lmdb/LmdbHiveLoader.hpp"
#include "../../persist/lmdb/LmdbHiveStore.hpp"
#include "../../persist/lmdb/lmdbHiveRecords.hpp"
#include "../../util/Handle.hpp"

#include <map>
#include <string>
#include <vector>

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

namespace
{
	/**
	 * A database file in a directory of its own, both removed when this goes out of scope.
	 */
	class PersistTestDatabase
	{
		public:

			PersistTestDatabase()
			{
				char dirTemplate[] = "/tmp/matrytsyaPersistTestXXXXXX";

				char* dir = mkdtemp(dirTemplate);

				if(dir) _dir = dir;

				path = _dir + "/hives.mdb";
			}

			~PersistTestDatabase()
			{
				unlink(path.c_str());
				unlink((path + "-lock").c_str());

				if(!_dir.empty()) rmdir(_dir.c_str());
			}

			/// Path of the database file.
			std::string path;

		private:

			/// Directory the database file is in.
			std::string _dir;
	};

	/**
	 * Open a store, handing back the handle that holds it.
	 */
	Handle<LmdbHiveStore> _openPersistTestStore(const std::string& path)
	{
		LmdbHiveStore* store = new LmdbHiveStore(path);

		Handle<LmdbHiveStore> storeHandle(store);

		store -> decrRef();

		return storeHandle;
	}

	/**
	 * Create a persister, handing back the handle that holds it.
	 */
	Handle<HivePersister> _createPersistTestPersister(Handle<LmdbHiveStore> store, Handle<GraphHive> hive,
		HiveLoader& loader)
	{
		HivePersister* persister = new HivePersister(store, hive, loader);

		Handle<HivePersister> persisterHandle(persister);

		persister -> decrRef();

		return persisterHandle;
	}

	/**
	 * Run a builder's core script once.
	 * @returns Whether the script ran without raising.
	 */
	bool _runPersistTestBuilder(GraphHive* hive, const std::string& builderName)
	{
		Handle<GraphNode> node = hive -> getNode(builderName);

		Handle<ScriptSession> sessionHandle =
			dynamic_cast<BuilderScriptNode*>(node.getInstance()) -> requestCoreSession();

		return sessionHandle.getInstance() -> run();
	}

	/**
	 * Find a node's descriptor in what a loader read.
	 * @returns The descriptor. One with an empty name if the loader holds no node of that name.
	 */
	HiveNodeDescriptor _findPersistTestNode(HiveLoader& loader, const std::string& name)
	{
		for(unsigned i = 0; i < loader.getNodeCount(); i++)
		{
			if(loader.getNode(i).name == name) return loader.getNode(i);
		}

		return HiveNodeDescriptor{};
	}

	/// A hive holding a builder that a test drives by running its core script. Wired so that its scene root
	/// reaches the builder, and so that every node type the builder below creates has a counterpart to compare.
	const char* const _PERSIST_TEST_HIVE_JSON = R"({
		"name": "PersistHive",
		"nodes": [
			{ "type": "SceneRootNode", "name": "root", "edges": [ { "toNodeName": "existing" } ] },
			{ "type": "PingNode", "name": "existing", "pokeEnabled": true,
				"edges": [ { "toNodeName": "root", "actionFlags": ["PING_GRAPH_ACTION"], "traversalKey": 3 } ] },
			{ "type": "BuilderScriptNode", "name": "builder", "coreScript": "createPingNode{ name = 'fresh' } createEdge('fresh', 'existing', { actionFlags = { ActionFlag.TRIGGER_GRAPH_ACTION }, actionsCompleteAfterTraverse = true }) createEdge('existing', 'fresh') createSceneRootNode{ name = 'freshRoot', strobePeriodMs = 70 } createSceneTransformNode{ name = 'freshTransform', transform = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1} }", "pokeScript": "" }
		],
		"surfaces": [
			{ "type": "GraphHiveSceneSurface", "name": "scene", "sceneRootNodeName": "root",
				"initialFocusNodeName": "existing", "focusViewportFraction": 0.25, "default": true },
			{ "type": "GraphHiveGraphViewSurface", "name": "structure" }
		],
		"strobeEmitters": [ { "nodeName": "root", "periodMs": 40 } ],
		"strobeSurfaces": [ { "surfaceName": "structure", "periodMs": 250 } ]
	})";
}

// -- JsonHiveWriter --

/**
 * Every node type written out reads back as what was written: writing what was read back gives the same text,
 * and the fields particular to each type survive.
 */
TEST(JsonHiveWriterTest, RoundTripsEveryNodeType)
{
	std::vector<HiveNodeDescriptor> descriptors;

	HiveEdgeDescriptor edge;
	edge.toNodeName = "elsewhere";
	edge.actionFlagNames = {"PING_GRAPH_ACTION", "TRIGGER_GRAPH_ACTION"};
	edge.actionsCompleteAfterTraverse = true;
	edge.traversalKey = 9;

	HiveNodeDescriptor ping{};
	ping.type = HiveNodeDescriptor::PING;
	ping.name = "ping";
	ping.pokeEnabled = true;
	ping.edges.push_back(edge);
	descriptors.push_back(ping);

	HiveNodeDescriptor teleport{};
	teleport.type = HiveNodeDescriptor::TELEPORT;
	teleport.name = "teleport";
	teleport.destination.setHostname("example.org");
	teleport.destination.setPort(8123);
	teleport.destination.setHiveName("Other");
	teleport.destination.setNodeName("target");
	descriptors.push_back(teleport);

	HiveNodeDescriptor root{};
	root.type = HiveNodeDescriptor::SCENE_ROOT;
	root.name = "root";
	descriptors.push_back(root);

	Vertex vertex{};
	vertex.posn[0] = 0.125;
	vertex.posn[1] = -3.5;
	vertex.posn[2] = 1e-7;
	vertex.colour[0] = std::byte{10};
	vertex.colour[3] = std::byte{255};
	vertex.texCoords[1] = 0.75;
	vertex.normal[2] = 1.0;

	HiveNodeDescriptor geometry{};
	geometry.type = HiveNodeDescriptor::SCENE_GEOMETRY;
	geometry.name = "geometry";
	geometry.emitAgentAffectAction = true;
	geometry.vertexGroups.push_back(HiveVertexGroupDescriptor{{vertex, vertex}, "ALWAYS"});
	geometry.vertexGroups.push_back(HiveVertexGroupDescriptor{{vertex}, "HOVERED_OVER"});
	descriptors.push_back(geometry);

	HiveNodeDescriptor geometryScript = geometry;
	geometryScript.type = HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT;
	geometryScript.name = "geometryScript";
	geometryScript.coreScript = "x = \"quoted\"\n";
	geometryScript.pokeScript = "y = 1";
	descriptors.push_back(geometryScript);

	HiveNodeDescriptor transform{};
	transform.type = HiveNodeDescriptor::SCENE_TRANSFORM;
	transform.name = "transform";
	transform.hasTransform = true;
	for(int i = 0; i < 16; i++) transform.transform[i] = i * 0.1;
	descriptors.push_back(transform);

	HiveNodeDescriptor transformScript{};
	transformScript.type = HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT;
	transformScript.name = "transformScript";
	transformScript.coreScript = "a";
	transformScript.pokeScript = "b";
	descriptors.push_back(transformScript);

	HiveNodeDescriptor text{};
	text.type = HiveNodeDescriptor::SCENE_TEXT_SCRIPT;
	text.name = "text";
	text.coreScript = "c";
	text.pokeScript = "d";
	text.text = "hello\nworld";
	text.textPosition[1] = 2.5;
	text.textHeight = 0.5;
	text.textColour[2] = std::byte{7};
	text.textBackground.padding = 0.25;
	text.textBackground.cornerRadii[TextBackground::BOTTOM_LEFT] = 0.5;
	text.textBackground.fillColour[0] = std::byte{200};
	text.textVisible = false;
	descriptors.push_back(text);

	HiveNodeDescriptor agent{};
	agent.type = HiveNodeDescriptor::AGENT;
	agent.name = "agent";
	agent.capabilityName = "HIGH";
	agent.prompts.push_back(HiveAgentPromptDescriptor{"ping", "PING_NODE", "go", true});
	agent.autoTriggerAgentAction = false;
	agent.serialiseEmittedActions = false;
	descriptors.push_back(agent);

	HiveNodeDescriptor trigger{};
	trigger.type = HiveNodeDescriptor::TRIGGER;
	trigger.name = "trigger";
	trigger.coreScript = "e";
	trigger.pokeScript = "f";
	trigger.emitTriggerOnPoke = false;
	descriptors.push_back(trigger);

	HiveNodeDescriptor builder{};
	builder.type = HiveNodeDescriptor::BUILDER_SCRIPT;
	builder.name = "builder";
	builder.coreScript = "g";
	builder.pokeScript = "h";
	builder.buildOnce = true;
	builder.built = true;
	descriptors.push_back(builder);

	for(const HiveNodeDescriptor& written : descriptors)
	{
		std::string fragment = JsonHiveWriter::nodeFragment(written);

		HiveNodeDescriptor read = JsonHiveLoader::parseNodeFragment(fragment);

		EXPECT_EQ(JsonHiveWriter::nodeFragment(read), fragment) << written.name;
		EXPECT_EQ(read.type, written.type) << written.name;
		EXPECT_EQ(read.name, written.name);
		EXPECT_EQ(read.pokeEnabled, written.pokeEnabled) << written.name;
		EXPECT_EQ(read.edges.size(), written.edges.size()) << written.name;
		EXPECT_EQ(read.coreScript, written.coreScript) << written.name;
		EXPECT_EQ(read.pokeScript, written.pokeScript) << written.name;
	}

	HiveNodeDescriptor readPing = JsonHiveLoader::parseNodeFragment(JsonHiveWriter::nodeFragment(ping));
	ASSERT_EQ(readPing.edges.size(), 1u);
	EXPECT_EQ(readPing.edges[0].toNodeName, "elsewhere");
	EXPECT_EQ(readPing.edges[0].actionFlagNames, edge.actionFlagNames);
	EXPECT_TRUE(readPing.edges[0].actionsCompleteAfterTraverse);
	EXPECT_EQ(readPing.edges[0].traversalKey, 9u);

	HiveNodeDescriptor readTeleport = JsonHiveLoader::parseNodeFragment(JsonHiveWriter::nodeFragment(teleport));
	EXPECT_EQ(readTeleport.destination.getHostname(), "example.org");
	EXPECT_EQ(readTeleport.destination.getPort(), 8123);
	EXPECT_EQ(readTeleport.destination.getNodeName(), "target");

	HiveNodeDescriptor readGeometry = JsonHiveLoader::parseNodeFragment(JsonHiveWriter::nodeFragment(geometry));
	ASSERT_EQ(readGeometry.vertexGroups.size(), 2u);
	EXPECT_EQ(readGeometry.vertexGroups[1].visibilityName, "HOVERED_OVER");
	EXPECT_EQ(readGeometry.vertexGroups[0].vertexes.size(), 2u);
	EXPECT_EQ(readGeometry.vertexGroups[0].vertexes[0].posn[2], 1e-7) << "Doubles must survive exactly.";
	EXPECT_EQ(readGeometry.vertexGroups[0].vertexes[0].colour[0], std::byte{10});
	EXPECT_TRUE(readGeometry.emitAgentAffectAction);

	HiveNodeDescriptor readTransform = JsonHiveLoader::parseNodeFragment(JsonHiveWriter::nodeFragment(transform));
	EXPECT_TRUE(readTransform.hasTransform);
	for(int i = 0; i < 16; i++) EXPECT_EQ(readTransform.transform[i], i * 0.1);

	HiveNodeDescriptor readTransformScript =
		JsonHiveLoader::parseNodeFragment(JsonHiveWriter::nodeFragment(transformScript));
	EXPECT_FALSE(readTransformScript.hasTransform) << "A node given no transform should read back as given none.";

	HiveNodeDescriptor readText = JsonHiveLoader::parseNodeFragment(JsonHiveWriter::nodeFragment(text));
	EXPECT_EQ(readText.text, "hello\nworld");
	EXPECT_EQ(readText.textPosition[1], 2.5);
	EXPECT_EQ(readText.textHeight, 0.5);
	EXPECT_EQ(readText.textColour[2], std::byte{7});
	EXPECT_EQ(readText.textBackground, text.textBackground);
	EXPECT_FALSE(readText.textVisible);

	HiveNodeDescriptor readAgent = JsonHiveLoader::parseNodeFragment(JsonHiveWriter::nodeFragment(agent));
	EXPECT_EQ(readAgent.capabilityName, "HIGH");
	ASSERT_EQ(readAgent.prompts.size(), 1u);
	EXPECT_EQ(readAgent.prompts[0].nodeIdentifier, "ping");
	EXPECT_EQ(readAgent.prompts[0].nodeTypeName, "PING_NODE");
	EXPECT_TRUE(readAgent.prompts[0].terminateOnResponse);
	EXPECT_FALSE(readAgent.autoTriggerAgentAction);
	EXPECT_FALSE(readAgent.serialiseEmittedActions);

	HiveNodeDescriptor readTrigger = JsonHiveLoader::parseNodeFragment(JsonHiveWriter::nodeFragment(trigger));
	EXPECT_FALSE(readTrigger.emitTriggerOnPoke);

	HiveNodeDescriptor readBuilder = JsonHiveLoader::parseNodeFragment(JsonHiveWriter::nodeFragment(builder));
	EXPECT_TRUE(readBuilder.buildOnce);
	EXPECT_TRUE(readBuilder.built);
}

/**
 * Surfaces and strobe registrations written out read back as what was written.
 */
TEST(JsonHiveWriterTest, RoundTripsSurfacesAndStrobes)
{
	HiveSurfaceDescriptor scene{};
	scene.type = HiveSurfaceDescriptor::SCENE_SURFACE;
	scene.name = "scene";
	scene.sceneRootNodeName = "root";
	scene.initialFocusNodeName = "focus";
	scene.focusViewportFraction = 0.3;
	scene.isDefault = true;

	HiveSurfaceDescriptor readScene = JsonHiveLoader::parseSurfaceFragment(JsonHiveWriter::surfaceFragment(scene));
	EXPECT_EQ(readScene.type, HiveSurfaceDescriptor::SCENE_SURFACE);
	EXPECT_EQ(readScene.name, "scene");
	EXPECT_EQ(readScene.sceneRootNodeName, "root");
	EXPECT_EQ(readScene.initialFocusNodeName, "focus");
	EXPECT_EQ(readScene.focusViewportFraction, 0.3);
	EXPECT_TRUE(readScene.isDefault);

	HiveSurfaceDescriptor view{};
	view.type = HiveSurfaceDescriptor::GRAPH_VIEW_SURFACE;
	view.name = "view";

	HiveSurfaceDescriptor readView = JsonHiveLoader::parseSurfaceFragment(JsonHiveWriter::surfaceFragment(view));
	EXPECT_EQ(readView.type, HiveSurfaceDescriptor::GRAPH_VIEW_SURFACE);
	EXPECT_FALSE(readView.isDefault);

	std::string name;
	unsigned periodMs = 0;

	JsonHiveLoader::parseStrobeEmitterFragment(JsonHiveWriter::strobeEmitterFragment("emitter", 17), name, periodMs);
	EXPECT_EQ(name, "emitter");
	EXPECT_EQ(periodMs, 17u);

	JsonHiveLoader::parseStrobeSurfaceFragment(JsonHiveWriter::strobeSurfaceFragment("surface", 19), name, periodMs);
	EXPECT_EQ(name, "surface");
	EXPECT_EQ(periodMs, 19u);
}

// -- LmdbHiveStore --

/**
 * A store is created where there was none, holds what is written to it per hive, applies deletes, and still holds
 * it all once reopened.
 */
TEST(LmdbHiveStoreTest, CreatesWritesReadsAndReopens)
{
	PersistTestDatabase database;

	ASSERT_NE(access(database.path.c_str(), F_OK), 0) << "The database should not exist before it is opened.";

	{
		Handle<LmdbHiveStore> store = _openPersistTestStore(database.path);

		EXPECT_EQ(access(database.path.c_str(), F_OK), 0) << "Opening should create the database file.";
		EXPECT_TRUE(store.getInstance() -> getHiveNames().empty());
		EXPECT_FALSE(store.getInstance() -> hasHive("A"));
		EXPECT_TRUE(store.getInstance() -> readRecords("A").empty());

		store.getInstance() -> writeRecords("A", {{"one", "1"}, {"two", "2"}}, {});
		store.getInstance() -> writeRecords("B", {{"three", "3"}}, {});
		store.getInstance() -> writeRecords("A", {{"two", "22"}}, {"one", "absent"});

		std::map<std::string, std::string> expectedA{{"two", "22"}};

		EXPECT_EQ(store.getInstance() -> readRecords("A"), expectedA);
		EXPECT_EQ(store.getInstance() -> getHiveNames(), (std::vector<std::string>{"A", "B"}));
	}

	Handle<LmdbHiveStore> reopened = _openPersistTestStore(database.path);

	std::map<std::string, std::string> expectedB{{"three", "3"}};

	EXPECT_TRUE(reopened.getInstance() -> hasHive("A"));
	EXPECT_EQ(reopened.getInstance() -> readRecords("B"), expectedB);
}

/**
 * A database that cannot be created is reported as such.
 */
TEST(LmdbHiveStoreTest, ReportsADatabaseThatCannotBeOpened)
{
	try
	{
		_openPersistTestStore("/nonexistent/directory/hives.mdb");

		FAIL() << "Opening should have failed.";
	}
	catch(PersistException& exception)
	{
		EXPECT_EQ(exception.getError(), PersistException::LMDB_OPEN_FAILED);
		EXPECT_FALSE(exception.getDetail().empty());
	}
}

// -- LmdbHiveLoader --

/**
 * A hive whose records disagree with its meta record is reported as corrupt rather than loaded short.
 */
TEST(LmdbHiveLoaderTest, ReportsCorruptRecords)
{
	PersistTestDatabase database;
	Handle<LmdbHiveStore> store = _openPersistTestStore(database.path);

	HiveNodeDescriptor ping{};
	ping.type = HiveNodeDescriptor::PING;
	ping.name = "ping";

	std::string meta = lmdbHiveRecords::metaRecord("Hive", {"ping"}, {});

	store.getInstance() -> writeRecords("Hive", {{lmdbHiveRecords::META_KEY, meta}}, {});

	EXPECT_THROW(LmdbHiveLoader(*store.getInstance(), "Hive"), PersistException) << "A missing node record.";

	store.getInstance() -> writeRecords("Hive", {{lmdbHiveRecords::nodeKey("ping"),
		JsonHiveWriter::nodeFragment(ping)}, {lmdbHiveRecords::nodeKey("stray"), JsonHiveWriter::nodeFragment(ping)}},
		{});

	try
	{
		LmdbHiveLoader loader(*store.getInstance(), "Hive");

		FAIL() << "A node record the meta record does not name should be reported.";
	}
	catch(PersistException& exception)
	{
		EXPECT_EQ(exception.getError(), PersistException::LMDB_CORRUPT_RECORD);
	}

	store.getInstance() -> writeRecords("Hive", {}, {lmdbHiveRecords::nodeKey("stray")});

	LmdbHiveLoader loader(*store.getInstance(), "Hive");

	EXPECT_EQ(loader.getNodeCount(), 1u);

	store.getInstance() -> writeRecords("Hive", {{lmdbHiveRecords::META_KEY,
		R"({"formatVersion":99,"name":"Hive","nodeOrder":[],"surfaceOrder":[]})"}}, {});

	try
	{
		LmdbHiveLoader unsupported(*store.getInstance(), "Hive");

		FAIL() << "A format version this does not read should be reported.";
	}
	catch(PersistException& exception)
	{
		EXPECT_EQ(exception.getError(), PersistException::LMDB_UNSUPPORTED_FORMAT);
	}
}

// -- HivePersister --

/**
 * A hive loaded from a hive file and stored reads back as the same hive, and rebuilds from what was stored.
 */
TEST(HivePersisterTest, StoresAHiveThatRebuildsTheSame)
{
	PersistTestDatabase database;
	Handle<LmdbHiveStore> store = _openPersistTestStore(database.path);

	JsonHiveLoader jsonLoader(_PERSIST_TEST_HIVE_JSON);

	GraphHive* hive = HiveBuilder::build(jsonLoader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<HivePersister> persister = _createPersistTestPersister(store, hiveHandle, jsonLoader);

	persister.getInstance() -> flush();

	LmdbHiveLoader stored(*store.getInstance(), "PersistHive");

	ASSERT_EQ(stored.getHiveName(), "PersistHive");
	ASSERT_EQ(stored.getNodeCount(), jsonLoader.getNodeCount());

	for(unsigned i = 0; i < jsonLoader.getNodeCount(); i++)
	{
		EXPECT_EQ(stored.getNode(i).name, jsonLoader.getNode(i).name) << "Node order should be kept.";
		EXPECT_EQ(JsonHiveWriter::nodeFragment(stored.getNode(i)), JsonHiveWriter::nodeFragment(jsonLoader.getNode(i)));
	}

	ASSERT_EQ(stored.getSurfaceCount(), 2u);
	EXPECT_EQ(stored.getSurface(0).name, "scene");
	EXPECT_EQ(stored.getSurface(0).initialFocusNodeName, "existing");
	EXPECT_EQ(stored.getSurface(1).name, "structure");

	ASSERT_EQ(stored.getStrobeEmitterCount(), 1u);
	ASSERT_EQ(stored.getStrobeSurfaceCount(), 1u);

	std::string name;
	unsigned periodMs = 0;

	stored.getStrobeSurface(0, name, periodMs);
	EXPECT_EQ(name, "structure");
	EXPECT_EQ(periodMs, 250u);

	GraphHive* rebuilt = HiveBuilder::build(stored, 2);
	Handle<GraphHive> rebuiltHandle(rebuilt);

	EXPECT_EQ(rebuilt -> getNodeNames().size(), 3u);
	EXPECT_EQ(rebuilt -> getSurfaceNames().size(), 2u);

	persister.getInstance() -> stop();

	hive -> shutdown();
	rebuilt -> shutdown();
}

/**
 * What a builder builds while the hive runs is stored: the nodes, the edges to and from them, a strobe
 * registration, and the build having been used up, so the rebuilt builder does not build it all again.
 */
TEST(HivePersisterTest, StoresWhatABuilderBuilds)
{
	PersistTestDatabase database;
	Handle<LmdbHiveStore> store = _openPersistTestStore(database.path);

	JsonHiveLoader jsonLoader(_PERSIST_TEST_HIVE_JSON);

	GraphHive* hive = HiveBuilder::build(jsonLoader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<HivePersister> persister = _createPersistTestPersister(store, hiveHandle, jsonLoader);

	persister.getInstance() -> flush();
	persister.getInstance() -> start();

	ASSERT_TRUE(_runPersistTestBuilder(hive, "builder"));
	ASSERT_TRUE(hive -> getNode("fresh").isValid());

	persister.getInstance() -> stop();

	LmdbHiveLoader stored(*store.getInstance(), "PersistHive");

	ASSERT_EQ(stored.getNodeCount(), 6u);
	EXPECT_EQ(stored.getNode(3).name, "fresh") << "Nodes added later should follow those loaded, in order added.";

	HiveNodeDescriptor fresh = _findPersistTestNode(stored, "fresh");
	EXPECT_EQ(fresh.type, HiveNodeDescriptor::PING);
	ASSERT_EQ(fresh.edges.size(), 1u);
	EXPECT_EQ(fresh.edges[0].toNodeName, "existing");
	EXPECT_EQ(fresh.edges[0].actionFlagNames, std::vector<std::string>{"TRIGGER_GRAPH_ACTION"});
	EXPECT_TRUE(fresh.edges[0].actionsCompleteAfterTraverse);

	HiveNodeDescriptor existing = _findPersistTestNode(stored, "existing");
	ASSERT_EQ(existing.edges.size(), 2u) << "An edge added to a loaded node should be stored too.";
	EXPECT_EQ(existing.edges[0].traversalKey, 3u);
	EXPECT_EQ(existing.edges[1].toNodeName, "fresh");

	EXPECT_TRUE(_findPersistTestNode(stored, "builder").built);

	bool foundFreshRoot = false;

	for(unsigned i = 0; i < stored.getStrobeEmitterCount(); i++)
	{
		std::string name;
		unsigned periodMs = 0;

		stored.getStrobeEmitter(i, name, periodMs);

		if(name == "freshRoot")
		{
			foundFreshRoot = true;
			EXPECT_EQ(periodMs, 70u);
		}
	}

	EXPECT_TRUE(foundFreshRoot) << "A strobe registration made by the builder should be stored.";

	hive -> shutdown();

	GraphHive* rebuilt = HiveBuilder::build(stored, 2);
	Handle<GraphHive> rebuiltHandle(rebuilt);

	Handle<GraphNode> rebuiltBuilder = rebuilt -> getNode("builder");
	EXPECT_TRUE(dynamic_cast<BuilderScriptNode*>(rebuiltBuilder.getInstance()) -> getBuilt());

	EXPECT_TRUE(_runPersistTestBuilder(rebuilt, "builder"));
	EXPECT_EQ(rebuilt -> getNodeNames().size(), 6u) << "A rebuilt builder should not build again.";
	EXPECT_EQ(rebuilt -> getNode("existing").getInstance() -> getEdges().size(), 2u);

	rebuilt -> shutdown();
}

/**
 * A node is stored as it was when it joined the hive, not as a script has since changed it.
 */
TEST(HivePersisterTest, StoresNodesAsTheyJoined)
{
	PersistTestDatabase database;
	Handle<LmdbHiveStore> store = _openPersistTestStore(database.path);

	JsonHiveLoader jsonLoader(_PERSIST_TEST_HIVE_JSON);

	GraphHive* hive = HiveBuilder::build(jsonLoader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<HivePersister> persister = _createPersistTestPersister(store, hiveHandle, jsonLoader);

	persister.getInstance() -> start();

	ASSERT_TRUE(_runPersistTestBuilder(hive, "builder"));

	Handle<GraphNode> freshTransform = hive -> getNode("freshTransform");
	ASSERT_TRUE(freshTransform.isValid());

	Transform moved = {9, 0, 0, 0, 0, 9, 0, 0, 0, 0, 9, 0, 0, 0, 0, 1};
	dynamic_cast<SceneTransformNode*>(freshTransform.getInstance()) -> setTransform(moved);

	persister.getInstance() -> stop();

	LmdbHiveLoader storedLoader(*store.getInstance(), "PersistHive");

	HiveNodeDescriptor stored = _findPersistTestNode(storedLoader, "freshTransform");

	ASSERT_EQ(stored.name, "freshTransform");
	EXPECT_TRUE(stored.hasTransform);
	EXPECT_EQ(stored.transform[0], 2.0) << "The transform the node was created with should be stored.";
	EXPECT_EQ(stored.transform[5], 2.0);

	hive -> shutdown();
}

/**
 * Changes are written while the hive runs, without waiting for the persister to be stopped.
 */
TEST(HivePersisterTest, WritesChangesWhileRunning)
{
	PersistTestDatabase database;
	Handle<LmdbHiveStore> store = _openPersistTestStore(database.path);

	JsonHiveLoader jsonLoader(_PERSIST_TEST_HIVE_JSON);

	GraphHive* hive = HiveBuilder::build(jsonLoader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<HivePersister> persister = _createPersistTestPersister(store, hiveHandle, jsonLoader);

	persister.getInstance() -> flush();
	persister.getInstance() -> start();

	ASSERT_TRUE(_runPersistTestBuilder(hive, "builder"));

	std::string freshKey = lmdbHiveRecords::nodeKey("fresh");

	bool written = false;

	// Well beyond the settle interval, so that only a writer that never writes fails this.
	for(int attempt = 0; attempt < 50 && !written; attempt++)
	{
		struct timespec pause{0, 100000000L};
		nanosleep(&pause, 0);

		written = store.getInstance() -> readRecords("PersistHive").count(freshKey) == 1;
	}

	EXPECT_TRUE(written) << "The builder's node should have been written while the hive was running.";

	persister.getInstance() -> stop();

	hive -> shutdown();
}

/**
 * A hive that has been shut down holds no nodes, which must not be stored as the hive having lost them all.
 */
TEST(HivePersisterTest, LeavesAShutDownHiveStoredAsItWas)
{
	PersistTestDatabase database;
	Handle<LmdbHiveStore> store = _openPersistTestStore(database.path);

	JsonHiveLoader jsonLoader(_PERSIST_TEST_HIVE_JSON);

	GraphHive* hive = HiveBuilder::build(jsonLoader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<HivePersister> persister = _createPersistTestPersister(store, hiveHandle, jsonLoader);

	persister.getInstance() -> flush();

	std::map<std::string, std::string> before = store.getInstance() -> readRecords("PersistHive");

	hive -> shutdown();

	persister.getInstance() -> stop();

	EXPECT_EQ(store.getInstance() -> readRecords("PersistHive"), before);
}

#endif
