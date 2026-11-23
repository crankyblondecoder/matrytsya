#ifndef HIVE_RECORDER_UNIT_TEST_H
#define HIVE_RECORDER_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphNode.hpp"
#include "../../graph/nodes/PingNode.hpp"
#include "../../persist/HiveBuilder.hpp"
#include "../../persist/HivePersister.hpp"
#include "../../persist/HiveRecorder.hpp"
#include "../../persist/HiveSnapshot.hpp"
#include "../../persist/json/JsonHiveLoader.hpp"
#include "../../persist/json/JsonHiveWriter.hpp"
#include "../../persist/lmdb/LmdbHiveStore.hpp"
#include "../../persist/lmdb/lmdbHiveRecords.hpp"
#include "../../util/Handle.hpp"

#include <map>
#include <string>

#include <stdlib.h>
#include <unistd.h>

namespace
{
	const char* const _RECORDER_TEST_HIVE_JSON = R"({
		"name": "RecorderHive",
		"nodes": [
			{ "type": "SceneRootNode", "name": "root", "edges": [ { "toNodeName": "a" } ] },
			{ "type": "PingNode", "name": "a",
				"edges": [ { "toNodeName": "b", "actionFlags": ["PING_GRAPH_ACTION"] } ] },
			{ "type": "PingNode", "name": "b", "pokeEnabled": true }
		],
		"surfaces": [ { "type": "GraphHiveGraphViewSurface", "name": "structure" } ],
		"strobeEmitters": [ { "nodeName": "root", "periodMs": 1000 } ],
		"strobeSurfaces": [ { "surfaceName": "structure", "periodMs": 500 } ]
	})";

	/**
	 * A database file in a directory of its own, both removed when this goes out of scope.
	 */
	class RecorderTestDatabase
	{
		public:

			RecorderTestDatabase()
			{
				char dirTemplate[] = "/tmp/matrytsyaRecorderTestXXXXXX";

				char* dir = mkdtemp(dirTemplate);

				if(dir) _dir = dir;

				path = _dir + "/hives.mdb";
			}

			~RecorderTestDatabase()
			{
				unlink(path.c_str());
				unlink((path + "-lock").c_str());

				if(!_dir.empty()) rmdir(_dir.c_str());
			}

			std::string path;

		private:

			std::string _dir;
	};

	Handle<HiveRecorder> _createRecorder(Handle<GraphHive> hive, HiveLoader& loader)
	{
		HiveRecorder* recorder = new HiveRecorder(hive, loader);

		Handle<HiveRecorder> recorderHandle(recorder);

		recorder -> decrRef();

		return recorderHandle;
	}
}

/**
 * A whole hive written as one document reads back as the same hive, and writes out the same again.
 */
TEST(HiveRecorderTest, HiveDocumentRoundTrips)
{
	JsonHiveLoader loader(_RECORDER_TEST_HIVE_JSON);

	GraphHive* hive = HiveBuilder::build(loader, 1);
	Handle<GraphHive> hiveHandle(hive);

	Handle<HiveRecorder> recorder = _createRecorder(hiveHandle, loader);

	std::string document = JsonHiveWriter::hiveDocument(recorder.getInstance() -> snapshot());

	JsonHiveLoader reread(document);

	EXPECT_EQ(reread.getHiveName(), "RecorderHive");
	ASSERT_EQ(reread.getNodeCount(), 3u);
	EXPECT_EQ(reread.getNode(1).name, "a");
	ASSERT_EQ(reread.getNode(1).edges.size(), 1u);
	EXPECT_EQ(reread.getNode(1).edges[0].toNodeName, "b");
	EXPECT_TRUE(reread.getNode(2).pokeEnabled);
	EXPECT_EQ(reread.getSurfaceCount(), 1u);
	EXPECT_EQ(reread.getStrobeEmitterCount(), 1u);
	EXPECT_EQ(reread.getStrobeSurfaceCount(), 1u);

	GraphHive* rebuilt = HiveBuilder::build(reread, 1);
	Handle<GraphHive> rebuiltHandle(rebuilt);

	Handle<HiveRecorder> rebuiltRecorder = _createRecorder(rebuiltHandle, reread);

	EXPECT_EQ(JsonHiveWriter::hiveDocument(rebuiltRecorder.getInstance() -> snapshot()), document);

	rebuilt -> shutdown();
	hive -> shutdown();
}

/**
 * A recorder notices nodes joining, leaving and being replaced while it listens, and its snapshot says so.
 */
TEST(HiveRecorderTest, TracksAddedRemovedAndReplacedNodes)
{
	JsonHiveLoader loader(_RECORDER_TEST_HIVE_JSON);

	GraphHive* hive = HiveBuilder::build(loader, 1);
	Handle<GraphHive> hiveHandle(hive);

	Handle<HiveRecorder> recorder = _createRecorder(hiveHandle, loader);

	recorder.getInstance() -> start();

	PingNode* added = new PingNode();
	added -> setName("added");
	ASSERT_TRUE(hive -> addUniquelyNamedNode(added));

	ASSERT_TRUE(hive -> removeNode(hive -> getNode("b")));

	HiveNodeDescriptor aDescriptor;
	ASSERT_TRUE(recorder.getInstance() -> getNode("a", aDescriptor));
	EXPECT_TRUE(aDescriptor.edges.empty()) << "a's edge into the removed b should be gone from its record.";

	aDescriptor.pokeEnabled = true;
	ASSERT_TRUE(hive -> replaceNode(hive -> getNode("a"), HiveBuilder::createNode(aDescriptor)));

	HiveSnapshot snapshot = recorder.getInstance() -> snapshot();

	std::vector<std::string> names;
	for(HiveNodeDescriptor& node : snapshot.nodes) names.push_back(node.name);

	EXPECT_EQ(names, (std::vector<std::string>{"root", "a", "added"})) << "A replaced node keeps its place.";

	HiveNodeDescriptor replaced;
	ASSERT_TRUE(recorder.getInstance() -> getNode("a", replaced));
	EXPECT_TRUE(replaced.pokeEnabled) << "The replacement should be recorded, not the original.";

	EXPECT_FALSE(recorder.getInstance() -> getNode("b", replaced));

	recorder.getInstance() -> stop();

	hive -> shutdown();
}

/**
 * A node removed from a stored hive is removed from the store, and a replaced node is stored as replaced.
 */
TEST(HiveRecorderTest, PersisterStoresRemovalAndReplacement)
{
	RecorderTestDatabase database;

	LmdbHiveStore* store = new LmdbHiveStore(database.path);
	Handle<LmdbHiveStore> storeHandle(store);
	store -> decrRef();

	JsonHiveLoader loader(_RECORDER_TEST_HIVE_JSON);

	GraphHive* hive = HiveBuilder::build(loader, 1);
	Handle<GraphHive> hiveHandle(hive);

	HivePersister* persister = new HivePersister(storeHandle, hiveHandle, loader);
	Handle<HivePersister> persisterHandle(persister);
	persister -> decrRef();

	persister -> flush();

	ASSERT_EQ(store -> readRecords("RecorderHive").count(lmdbHiveRecords::nodeKey("b")), 1u);

	ASSERT_TRUE(hive -> removeNode(hive -> getNode("b")));


	HiveNodeDescriptor rootDescriptor = JsonHiveLoader::parseNodeFragment(
		R"({ "type": "SceneRootNode", "name": "root" })");

	ASSERT_TRUE(hive -> replaceNode(hive -> getNode("root"), HiveBuilder::createNode(rootDescriptor)));

	persister -> flush();

	std::map<std::string, std::string> records = store -> readRecords("RecorderHive");

	EXPECT_EQ(records.count(lmdbHiveRecords::nodeKey("b")), 0u) << "The removed node's record should be deleted.";
	ASSERT_EQ(records.count(lmdbHiveRecords::nodeKey("root")), 1u);
	EXPECT_EQ(records.count(lmdbHiveRecords::strobeEmitterKey("root")), 1u)
		<< "The replaced root should still be a strobe emitter.";

	HiveNodeDescriptor storedA = JsonHiveLoader::parseNodeFragment(records.at(lmdbHiveRecords::nodeKey("a")));
	EXPECT_TRUE(storedA.edges.empty()) << "a's edge into the removed b should be gone from the store.";

	persister -> stop();

	hive -> shutdown();
}

/**
 * Deleting a hive from a store removes it and every record of it, and deleting one that is not there says so.
 */
TEST(HiveRecorderTest, StoreDeletesHive)
{
	RecorderTestDatabase database;

	LmdbHiveStore* store = new LmdbHiveStore(database.path);
	Handle<LmdbHiveStore> storeHandle(store);
	store -> decrRef();

	store -> writeRecords("Gone", {{"one", "1"}}, {});
	store -> writeRecords("Kept", {{"two", "2"}}, {});

	EXPECT_TRUE(store -> deleteHive("Gone"));
	EXPECT_FALSE(store -> deleteHive("Gone"));
	EXPECT_FALSE(store -> deleteHive("NeverThere"));

	EXPECT_FALSE(store -> hasHive("Gone"));
	EXPECT_TRUE(store -> readRecords("Gone").empty());
	EXPECT_EQ(store -> getHiveNames(), (std::vector<std::string>{"Kept"}));

	store -> writeRecords("Gone", {{"three", "3"}}, {});
	EXPECT_EQ(store -> readRecords("Gone").size(), 1u) << "A deleted hive's name can be stored again.";
}

#endif
