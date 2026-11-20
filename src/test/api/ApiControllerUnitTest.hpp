#ifndef API_CONTROLLER_UNIT_TEST_H
#define API_CONTROLLER_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../api/ApiAuthenticator.hpp"
#include "../../api/ApiController.hpp"
#include "../../api/ApiHttpMap.hpp"
#include "../../api/HiveHost.hpp"
#include "../../display/GraphHiveCollectionHttpMap.hpp"
#include "../../display/http/HttpRequest.hpp"
#include "../../display/http/HttpResponse.hpp"
#include "../../display/http/HttpServerBase.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphHiveCollection.hpp"
#include "../../persist/json/JsonHiveLoader.hpp"
#include "../../persist/lmdb/LmdbHiveStore.hpp"
#include "../../rapidjson/document.h"
#include "../../util/Handle.hpp"

#include <memory>
#include <string>

#include <stdlib.h>
#include <unistd.h>

namespace
{
	const std::string _API_TEST_TOKEN = "0123456789abcdef0123456789abcdef-api";

	const char* const _API_TEST_HIVE_JSON = R"({
		"name": "ApiHive",
		"nodes": [
			{ "type": "SceneRootNode", "name": "root", "edges": [ { "toNodeName": "a" } ] },
			{ "type": "PingNode", "name": "a", "edges": [ { "toNodeName": "b", "actionFlags": ["PING_GRAPH_ACTION"] } ] },
			{ "type": "PingNode", "name": "b" },
			{ "type": "TriggerNode", "name": "trigger", "coreScript": "", "pokeScript": "" }
		],
		"surfaces": [
			{ "type": "GraphHiveSceneSurface", "name": "scene", "sceneRootNodeName": "root", "default": true },
			{ "type": "GraphHiveGraphViewSurface", "name": "structure" }
		],
		"strobeEmitters": [ { "nodeName": "root", "periodMs": 1000 } ]
	})";

	/**
	 * A server that never listens, whose routing a test drives directly.
	 */
	class ApiTestServer : public HttpServerBase
	{
		public:

			ApiTestServer() : HttpServerBase("127.0.0.1", 0) {}

			void route(HttpRequest& request, HttpResponse& response) { _routeRequest(request, response); }

		protected:

			void _start() override {}

			void _stop() override {}
	};

	/**
	 * A database file in a directory of its own, both removed when this goes out of scope.
	 */
	class ApiTestDatabase
	{
		public:

			ApiTestDatabase()
			{
				char dirTemplate[] = "/tmp/matrytsyaApiTestXXXXXX";

				char* dir = mkdtemp(dirTemplate);

				if(dir) _dir = dir;

				path = _dir + "/hives.mdb";
			}

			~ApiTestDatabase()
			{
				unlink(path.c_str());
				unlink((path + "-lock").c_str());

				if(!_dir.empty()) rmdir(_dir.c_str());
			}

			std::string path;

		private:

			std::string _dir;
	};

	/**
	 * Everything the API is served with, as the program sets it up, with one hive loaded.
	 */
	class ApiTestRig
	{
		public:

			ApiTestRig()
			{
				LmdbHiveStore* store = new LmdbHiveStore(_database.path);

				_store = Handle<LmdbHiveStore>(store);

				store -> decrRef();

				GraphHiveCollectionHttpMap* collectionMap = new GraphHiveCollectionHttpMap(server, collection, "/");

				_collectionMap = Handle<GraphHiveCollectionHttpMap>(collectionMap);

				collectionMap -> decrRef();

				host = std::make_unique<HiveHost>(collection, server, *collectionMap, _store,
					Handle<GraphToolBindingsFactory>(0), HiveHost::HarnessFactory(), HiveHost::Settings());

				JsonHiveLoader loader(_API_TEST_HIVE_JSON);

				host -> addHive(loader, false);

				std::string tokens = _API_TEST_TOKEN;

				_authenticator = std::make_unique<ApiAuthenticator>(tokens);
				_controller = std::make_unique<ApiController>(*host, collection);

				ApiHttpMap* apiMap = new ApiHttpMap(server, *_authenticator, *_controller);

				_apiMap = Handle<ApiHttpMap>(apiMap);

				apiMap -> decrRef();
			}

			~ApiTestRig()
			{
				_apiMap = Handle<ApiHttpMap>(0);

				for(std::string& name : collection.getHiveNames()) host -> removeHive(name, false);

				host -> shutdown();

				collection.shutdown();
			}

			/**
			 * Make a request of the API.
			 * @param method HTTP method.
			 * @param path Path, starting /api/v1.
			 * @param body Body, sent as JSON. Empty for none.
			 * @param query Query string, without the '?'.
			 * @param token Token to send. Empty to send none.
			 */
			HttpResponse request(const std::string& method, const std::string& path, const std::string& body = "",
				const std::string& query = "", const std::string& token = _API_TEST_TOKEN)
			{
				HttpRequest request(method, path, query, body);

				if(!token.empty()) request.setHeader("Authorization", "Bearer " + token);
				if(!body.empty()) request.setHeader("Content-Type", "application/json");

				request.setRemoteAddress("127.0.0.1");

				HttpResponse response;

				server.route(request, response);

				return response;
			}

			/**
			 * Make a request of the API and parse what it answers.
			 */
			rapidjson::Document json(const std::string& method, const std::string& path, const std::string& body = "",
				const std::string& query = "", unsigned* status = 0)
			{
				HttpResponse response = request(method, path, body, query);

				if(status) *status = response.getStatus();

				rapidjson::Document document;

				std::string text = response.getBody();

				document.Parse(text.c_str(), text.size());

				return document;
			}

			ApiTestServer server;

			GraphHiveCollection collection;

			std::unique_ptr<HiveHost> host;

		private:

			ApiTestDatabase _database;

			Handle<LmdbHiveStore> _store{0};

			Handle<GraphHiveCollectionHttpMap> _collectionMap{0};

			std::unique_ptr<ApiAuthenticator> _authenticator;

			std::unique_ptr<ApiController> _controller;

			Handle<ApiHttpMap> _apiMap{0};
	};

	std::string _errorCode(rapidjson::Document& document)
	{
		if(!document.IsObject() || !document.HasMember("error")) return "";

		return document["error"]["code"].GetString();
	}

	bool _hasEdgeTo(rapidjson::Value& node, const std::string& target)
	{
		for(rapidjson::Value& edge : node["edges"].GetArray())
		{
			if(target == edge["to"].GetString()) return true;
		}

		return false;
	}
}

/**
 * Nothing is served without a valid token, and every answer is JSON marked not to be cached.
 */
TEST(ApiControllerTest, RequiresToken)
{
	ApiTestRig rig;

	HttpResponse missing = rig.request("GET", "/api/v1/hives", "", "", "");

	EXPECT_EQ(missing.getStatus(), 401u);
	EXPECT_EQ(missing.getContentType(), "application/json");

	HttpResponse wrong = rig.request("GET", "/api/v1/hives", "", "", "not-the-token-not-the-token-not-the");

	EXPECT_EQ(wrong.getStatus(), 401u);

	HttpResponse right = rig.request("GET", "/api/v1/hives");

	EXPECT_EQ(right.getStatus(), 200u);

	bool noStore = false;

	for(std::pair<std::string, std::string>& header : right.getHeaders())
	{
		if(header.first == "Cache-Control" && header.second == "no-store") noStore = true;
	}

	EXPECT_TRUE(noStore);
}

/**
 * Routing answers unknown paths, wrong methods and bodies not declared as JSON with the right errors.
 */
TEST(ApiControllerTest, RejectsBadRequests)
{
	ApiTestRig rig;

	unsigned status = 0;

	rapidjson::Document notFound = rig.json("GET", "/api/v1/nowhere", "", "", &status);
	EXPECT_EQ(status, 404u);
	EXPECT_EQ(_errorCode(notFound), "NOT_FOUND");

	rapidjson::Document wrongMethod = rig.json("PUT", "/api/v1/hives", "{}", "", &status);
	EXPECT_EQ(status, 405u);
	EXPECT_EQ(_errorCode(wrongMethod), "METHOD_NOT_ALLOWED");

	HttpRequest plain("POST", "/api/v1/hives/ApiHive/nodes", "", "{\"type\":\"PingNode\",\"name\":\"x\"}");
	plain.setHeader("Authorization", "Bearer " + _API_TEST_TOKEN);
	plain.setHeader("Content-Type", "text/plain");

	HttpResponse plainResponse;
	rig.server.route(plain, plainResponse);
	EXPECT_EQ(plainResponse.getStatus(), 415u);

	rapidjson::Document notJson = rig.json("POST", "/api/v1/hives/ApiHive/nodes", "{nope", "", &status);
	EXPECT_EQ(status, 400u);
	EXPECT_EQ(_errorCode(notJson), "BAD_REQUEST");

	rapidjson::Document badType = rig.json("POST", "/api/v1/hives/ApiHive/nodes",
		R"({"type": "NoSuchNode", "name": "x"})", "", &status);
	EXPECT_EQ(status, 422u);
	EXPECT_EQ(_errorCode(badType), "UNPROCESSABLE");

	rapidjson::Document noHive = rig.json("GET", "/api/v1/hives/Missing/nodes", "", "", &status);
	EXPECT_EQ(status, 404u);
}

/**
 * Nodes and edges can be added, read, edited and removed, and the hive's version follows every change.
 */
TEST(ApiControllerTest, NodeAndEdgeLifecycle)
{
	ApiTestRig rig;

	unsigned status = 0;

	rapidjson::Document before = rig.json("GET", "/api/v1/hives/ApiHive");
	unsigned versionBefore = before["version"].GetUint();

	rapidjson::Document created = rig.json("POST", "/api/v1/hives/ApiHive/nodes",
		R"({"type": "PingNode", "name": "c", "edges": [ { "toNodeName": "a", "traversalKey": 2 }, { "toNodeName": "c" } ]})",
		"", &status);

	ASSERT_EQ(status, 201u);
	EXPECT_STREQ(created["type"].GetString(), "PingNode");
	EXPECT_TRUE(_hasEdgeTo(created, "a"));
	EXPECT_TRUE(_hasEdgeTo(created, "c")) << "A node may be created with an edge to itself.";

	rig.json("POST", "/api/v1/hives/ApiHive/nodes", R"({"type": "PingNode", "name": "c"})", "", &status);
	EXPECT_EQ(status, 409u) << "A second node of the same name should be refused.";

	rig.json("POST", "/api/v1/hives/ApiHive/nodes",
		R"({"type": "PingNode", "name": "d", "edges": [ { "toNodeName": "nowhere" } ]})", "", &status);
	EXPECT_EQ(status, 422u);
	EXPECT_EQ(rig.json("GET", "/api/v1/hives/ApiHive/nodes/d").HasMember("error"), true)
		<< "A refused node should not have been added.";

	rapidjson::Document edge = rig.json("POST", "/api/v1/hives/ApiHive/nodes/b/edges",
		R"({"to": "c", "actionFlags": ["TRIGGER_GRAPH_ACTION"], "actionsCompleteAfterTraverse": true})", "", &status);

	ASSERT_EQ(status, 201u);
	EXPECT_STREQ(edge["to"].GetString(), "c");
	EXPECT_STREQ(edge["actionFlags"][0].GetString(), "TRIGGER_GRAPH_ACTION");

	rig.json("POST", "/api/v1/hives/ApiHive/nodes/b/edges", R"({"to": "c", "actionFlags": ["VERSION_GRAPH_ACTION"]})",
		"", &status);
	EXPECT_EQ(status, 422u) << "The version action may not be named on an edge.";

	rapidjson::Document patched = rig.json("PATCH", "/api/v1/hives/ApiHive/nodes/c", R"({"pokeEnabled": true})", "",
		&status);

	ASSERT_EQ(status, 200u);
	EXPECT_TRUE(patched["definition"]["pokeEnabled"].GetBool());
	EXPECT_TRUE(_hasEdgeTo(patched, "a")) << "An edit should keep the node's own edges.";
	EXPECT_TRUE(_hasEdgeTo(patched, "c")) << "An edit should keep the node's self loop.";
	EXPECT_NE(patched["id"].GetUint(), created["id"].GetUint()) << "An edited node is a new node.";

	rapidjson::Document b = rig.json("GET", "/api/v1/hives/ApiHive/nodes/b");
	EXPECT_TRUE(_hasEdgeTo(b, "c")) << "An edit should keep edges into the node.";

	rig.json("PATCH", "/api/v1/hives/ApiHive/nodes/c", R"({"name": "renamed"})", "", &status);
	EXPECT_EQ(status, 422u);

	rig.json("PATCH", "/api/v1/hives/ApiHive/nodes/c", R"({"type": "SceneRootNode"})", "", &status);
	EXPECT_EQ(status, 422u);

	rapidjson::Document rewired = rig.json("PATCH", "/api/v1/hives/ApiHive/nodes/c",
		R"({"edges": [ { "toNodeName": "b" } ]})", "", &status);

	ASSERT_EQ(status, 200u);
	EXPECT_EQ(rewired["edges"].Size(), 1u) << "Giving edges should replace every edge leaving the node.";
	EXPECT_TRUE(_hasEdgeTo(rewired, "b"));

	unsigned edgeId = rewired["edges"][0]["id"].GetUint();

	rig.json("DELETE", "/api/v1/hives/ApiHive/nodes/c/edges/" + std::to_string(edgeId), "", "", &status);
	EXPECT_EQ(status, 200u);

	rig.json("DELETE", "/api/v1/hives/ApiHive/nodes/c/edges/" + std::to_string(edgeId), "", "", &status);
	EXPECT_EQ(status, 404u);

	rig.json("DELETE", "/api/v1/hives/ApiHive/nodes/c", "", "", &status);
	EXPECT_EQ(status, 200u);

	b = rig.json("GET", "/api/v1/hives/ApiHive/nodes/b");
	EXPECT_FALSE(_hasEdgeTo(b, "c")) << "Removing a node should remove edges into it.";

	rig.json("GET", "/api/v1/hives/ApiHive/nodes/c", "", "", &status);
	EXPECT_EQ(status, 404u);

	rapidjson::Document after = rig.json("GET", "/api/v1/hives/ApiHive");
	EXPECT_GT(after["version"].GetUint(), versionBefore);
}

/**
 * A node a surface is bound to cannot be changed or removed, and nor can a hive's last node.
 */
TEST(ApiControllerTest, ProtectsBoundAndLastNodes)
{
	ApiTestRig rig;

	unsigned status = 0;

	rapidjson::Document bound = rig.json("DELETE", "/api/v1/hives/ApiHive/nodes/root", "", "", &status);
	EXPECT_EQ(status, 409u);
	EXPECT_EQ(_errorCode(bound), "CONFLICT");

	rig.json("PATCH", "/api/v1/hives/ApiHive/nodes/root", R"({"pokeEnabled": true})", "", &status);
	EXPECT_EQ(status, 409u);

	rig.json("POST", "/api/v1/hives", R"({"name": "Lonely", "nodes": [ { "type": "PingNode", "name": "only" } ]})",
		"persist=false", &status);
	ASSERT_EQ(status, 201u);

	rig.json("DELETE", "/api/v1/hives/Lonely/nodes/only", "", "", &status);
	EXPECT_EQ(status, 409u);
}

/**
 * Runtime control requests reach the nodes they name, and refuse nodes that cannot do what is asked.
 */
TEST(ApiControllerTest, ControlsRunningHive)
{
	ApiTestRig rig;

	unsigned status = 0;

	rig.json("POST", "/api/v1/hives/ApiHive/nodes/a/trigger", R"({"nodeType": "TRIGGER_NODE"})", "", &status);
	EXPECT_EQ(status, 202u);

	rig.json("POST", "/api/v1/hives/ApiHive/nodes/a/trigger", "", "", &status);
	EXPECT_EQ(status, 202u) << "A trigger needs no body.";

	rig.json("POST", "/api/v1/hives/ApiHive/nodes/a/trigger", R"({"nodeType": "NOT_A_TYPE"})", "", &status);
	EXPECT_EQ(status, 422u);

	rig.json("POST", "/api/v1/hives/ApiHive/nodes/a/poke", R"({"type": "hoverEnter", "chunkId": 3})", "", &status);
	EXPECT_EQ(status, 202u);

	rig.json("POST", "/api/v1/hives/ApiHive/nodes/a/poke", R"({"type": "squeeze"})", "", &status);
	EXPECT_EQ(status, 422u);

	rig.json("POST", "/api/v1/hives/ApiHive/nodes/a/animate", R"({"animating": true})", "", &status);
	EXPECT_EQ(status, 422u) << "A ping node cannot be animated.";

	rig.json("PUT", "/api/v1/hives/ApiHive/strobeEmitters/a", R"({"periodMs": 10})", "", &status);
	EXPECT_EQ(status, 422u) << "Only a scene root can be a strobe emitter.";

	rig.json("PUT", "/api/v1/hives/ApiHive/strobeEmitters/root", R"({"periodMs": 0})", "", &status);
	EXPECT_EQ(status, 422u);

	rig.json("PUT", "/api/v1/hives/ApiHive/strobeEmitters/root", R"({"periodMs": 250})", "", &status);
	EXPECT_EQ(status, 200u);

	rig.json("PUT", "/api/v1/hives/ApiHive/strobeSurfaces/structure", R"({"periodMs": 300})", "", &status);
	EXPECT_EQ(status, 200u);

	rapidjson::Document hive = rig.json("GET", "/api/v1/hives/ApiHive");

	ASSERT_EQ(hive["strobeEmitters"].Size(), 1u);
	EXPECT_EQ(hive["strobeEmitters"][0]["periodMs"].GetUint(), 250u);
	ASSERT_EQ(hive["strobeSurfaces"].Size(), 1u);
	EXPECT_STREQ(hive["strobeSurfaces"][0]["surfaceName"].GetString(), "structure");

	rig.json("DELETE", "/api/v1/hives/ApiHive/strobeEmitters/root", "", "", &status);
	EXPECT_EQ(status, 200u);

	rig.json("DELETE", "/api/v1/hives/ApiHive/strobeEmitters/root", "", "", &status);
	EXPECT_EQ(status, 200u) << "Clearing should be safe to repeat.";

	rig.json("DELETE", "/api/v1/hives/ApiHive/strobeSurfaces/nowhere", "", "", &status);
	EXPECT_EQ(status, 404u);

	hive = rig.json("GET", "/api/v1/hives/ApiHive");
	EXPECT_EQ(hive["strobeEmitters"].Size(), 0u);
}

/**
 * A hive can be created, exported, recreated from its export, and deleted, its surfaces being served and then
 * unserved along the way.
 */
TEST(ApiControllerTest, HiveLifecycle)
{
	ApiTestRig rig;

	unsigned status = 0;

	rapidjson::Document definition = rig.json("GET", "/api/v1/hives/ApiHive/definition", "", "", &status);
	ASSERT_EQ(status, 200u);

	std::string exported = rig.request("GET", "/api/v1/hives/ApiHive/definition").getBody();

	// The export is itself a valid hive definition.
	JsonHiveLoader reloaded(exported);
	EXPECT_EQ(reloaded.getNodeCount(), 4u);

	std::string copy = exported;
	copy.replace(copy.find("\"ApiHive\""), 9, "\"ApiCopy\"");

	rapidjson::Document created = rig.json("POST", "/api/v1/hives", copy, "", &status);
	ASSERT_EQ(status, 201u);
	EXPECT_TRUE(created["persisted"].GetBool());

	std::string scenePath = created["surfaces"][0]["path"].GetString();
	EXPECT_EQ(scenePath, "/ApiCopy/scene/");

	HttpRequest page("GET", scenePath, "", "");
	HttpResponse pageResponse;
	rig.server.route(page, pageResponse);
	EXPECT_EQ(pageResponse.getStatus(), 200u);
	EXPECT_EQ(pageResponse.getContentType(), "text/html");

	EXPECT_EQ(rig.request("GET", "/api/v1/hives/ApiCopy/definition").getBody(),
		std::string(exported).replace(exported.find("\"ApiHive\""), 9, "\"ApiCopy\""))
		<< "A hive built from an export should export the same.";

	rig.json("POST", "/api/v1/hives", copy, "", &status);
	EXPECT_EQ(status, 409u);

	rapidjson::Document deleted = rig.json("DELETE", "/api/v1/hives/ApiCopy", "", "", &status);
	ASSERT_EQ(status, 200u);
	EXPECT_TRUE(deleted["deleted"].GetBool());
	EXPECT_FALSE(deleted["purged"].GetBool());

	HttpResponse gonePage;
	rig.server.route(page, gonePage);
	EXPECT_EQ(gonePage.getStatus(), 404u) << "A deleted hive's surfaces should no longer be served.";

	rig.json("POST", "/api/v1/hives", copy, "", &status);
	EXPECT_EQ(status, 409u) << "A hive still stored should not be overwritten by creating another of its name.";

	rapidjson::Document purged = rig.json("DELETE", "/api/v1/hives/ApiCopy", "", "purge=true", &status);
	ASSERT_EQ(status, 200u);
	EXPECT_FALSE(purged["deleted"].GetBool());
	EXPECT_TRUE(purged["purged"].GetBool());

	rig.json("POST", "/api/v1/hives", copy, "", &status);
	EXPECT_EQ(status, 201u) << "Once purged, the name can be used again.";

	rig.json("DELETE", "/api/v1/hives/ApiCopy", "", "purge=true", &status);
	EXPECT_EQ(status, 200u);

	rig.json("DELETE", "/api/v1/hives/ApiCopy", "", "purge=true", &status);
	EXPECT_EQ(status, 404u);

	rapidjson::Document list = rig.json("GET", "/api/v1/hives");
	ASSERT_EQ(list["hives"].Size(), 1u);
	EXPECT_STREQ(list["hives"][0]["name"].GetString(), "ApiHive");
}

/**
 * A hive definition that cannot be built is refused, and leaves nothing behind.
 */
TEST(ApiControllerTest, RefusesInvalidHive)
{
	ApiTestRig rig;

	unsigned status = 0;

	rapidjson::Document refused = rig.json("POST", "/api/v1/hives",
		R"({"name": "Broken", "nodes": [ { "type": "PingNode", "name": "a", "edges": [ { "toNodeName": "nowhere" } ] } ]})",
		"", &status);

	EXPECT_EQ(status, 422u);
	EXPECT_EQ(_errorCode(refused), "UNPROCESSABLE");

	rig.json("GET", "/api/v1/hives/Broken", "", "", &status);
	EXPECT_EQ(status, 404u);
}

#endif
