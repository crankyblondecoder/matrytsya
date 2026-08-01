#include "agent/AgenticHarness.hpp"
#include "agent/Model.hpp"
#include "agent_bindings/ModelToolBindingsFactory.hpp"
#include "display/DisplayException.hpp"
#include "display/GraphHiveCollectionHttpMap.hpp"
#include "display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "display/http/HttpServer.hpp"
#include "util/Handle.hpp"
#include "graph/GraphHive.hpp"
#include "graph/GraphHiveCollection.hpp"
#include "graph/GraphHiveSceneSurface.hpp"
#include "graph/GraphToolBindingsFactory.hpp"
#include "persist/HarnessBuilder.hpp"
#include "persist/HiveBuilder.hpp"
#include "persist/PersistException.hpp"
#include "persist/json/JsonHarnessLoader.hpp"
#include "persist/json/JsonHiveLoader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <signal.h>
#include <unistd.h>

namespace
{
	volatile sig_atomic_t _running = 1;

	void _handleSigInt(int)
	{
		_running = 0;
	}

	// Hives loaded into this test's collection, all served from the one server. Run from the build directory
	// (see build/makefile's debug_test target), so these are relative to that.
	const char* const _HIVE_JSON_PATHS[] =
	{
		"../examples/engineHive.json",
		"../examples/flowerHive.json"
		//,"../examples/roseHive.json"
	};

	const unsigned _WEBGL_POLL_INTERVAL_MS = 50;

	// Kept short rather than matched to how often a first request is actually expected, since this is also
	// how soon a SIGINT lands: waitForFirstRequest() runs a whole call out to completion once started, and
	// only the loop calling it re-checks _running in between.
	const unsigned _FIRST_REQUEST_WAIT_POLL_MS = 100;

	// Harness this test runs against: which server, which model, and the prompts and tools each role is
	// given. Relative to the build directory, as _HIVE_JSON_PATH is.
	const char* const _HARNESS_JSON_PATH = "../examples/ollamaHarness.json";

	// The capability the chat panel asks its requests at, which is a property of this map rather than of the
	// harness, so it has to be told what the harness file above assigns the chat role at.
	const AgenticHarness::Capability _CHAT_CAPABILITY = AgenticHarness::Capability::LOW;

	/// Path the collection index is mounted at, which is what a browser arriving at the server lands on.
	const char* const _INDEX_PATH = "/";

	std::string _readFile(const std::string& path)
	{
		std::ifstream file(path);

		if(!file.is_open())
		{
			std::cerr << "Could not open hive definition file: " << path << std::endl;
			exit(1);
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	/**
	 * Build the agentic harness described by the harness definition file, with its tool bindings taken from
	 * the factory and built against the hive.
	 * @param hive Hive the tool bindings are to report on.
	 * @param toolBindingsFactory Factory the tool bindings are taken from.
	 * @returns Handle to the harness, or an invalid handle when the file cannot be read or anything it asks
	 *          for cannot be honoured -- an unreachable server included -- in which case the hive is left to
	 *          chat with no model behind it.
	 * @note A failure here is reported and swallowed rather than fatal, unlike a hive that will not load:
	 *       everything else this test does works with no model behind it, and the server the harness file
	 *       names is not always up.
	 */
	Handle<AgenticHarness> _buildAgenticHarness(Handle<GraphHive> hive,
		Handle<GraphToolBindingsFactory> toolBindingsFactory)
	{
		std::ifstream file(_HARNESS_JSON_PATH);

		if(!file.is_open())
		{
			std::cerr << "Could not open harness definition file: " << _HARNESS_JSON_PATH
				<< " -- disabling model use." << std::endl;

			return Handle<AgenticHarness>(0);
		}

		std::stringstream buffer;
		buffer << file.rdbuf();

		try
		{
			JsonHarnessLoader loader(buffer.str());

			// Contacts every server the file names, so this blocks for as long as they take to answer.
			AgenticHarness* harness = HarnessBuilder::build(loader, hive, toolBindingsFactory);

			Handle<AgenticHarness> harnessHandle(harness);

			// The handle carries the reference out to the caller; release the implicit construction ref.
			harness -> decrRef();

			return harnessHandle;
		}
		catch(PersistException& exception)
		{
			std::cerr << "Could not build the harness described by " << _HARNESS_JSON_PATH << " (error "
				<< exception.getError() << ") -- disabling model use." << std::endl;

			return Handle<AgenticHarness>(0);
		}
	}
}

int main(int argc, char const *argv[])
{
	Model::_logToConsole = true;

	// Declared before anything that points back at it: every hive loaded below is given this collection, and
	// so is the index map, neither of which reference count it.
	GraphHiveCollection collection;

	HttpServer httpServer(8080);

	// Mounted at the root, so it is both what a browser arriving at the server lands on and what any path no
	// surface map claims falls through to. Routing is by longest matching prefix, so the surface maps mounted
	// below it are still reached.
	GraphHiveCollectionHttpMap* collectionMap =
		new GraphHiveCollectionHttpMap(httpServer, collection, _INDEX_PATH);

	Handle<GraphHiveCollectionHttpMap> collectionMapHandle(collectionMap);

	// Built here rather than by the hives themselves, as the concrete factory belongs to agent_bindings, which
	// depends on graph. It is not built against any one hive, so all of them share this one.
	ModelToolBindingsFactory* toolBindingsFactory = new ModelToolBindingsFactory();

	Handle<GraphToolBindingsFactory> toolBindingsFactoryHandle(toolBindingsFactory);

	// The handle holds the reference; release the implicit construction ref.
	toolBindingsFactory -> decrRef();

	// Held for as long as this test runs, so that neither a hive nor a map it serves is released while the
	// server is still answering for it.
	std::vector<Handle<GraphHive>> hiveHandles;
	std::vector<Handle<GraphHiveSceneSurfaceWebglMap>> webglMapHandles;

	for(const char* const hiveJsonPath : _HIVE_JSON_PATHS)
	{
		JsonHiveLoader loader(_readFile(hiveJsonPath));

		GraphHive* hive = HiveBuilder::build(loader, 2);
		Handle<GraphHive> hiveHandle(hive);

		hiveHandles.push_back(hiveHandle);

		// Both directions of the association are needed: the collection serves the hive up by name, and the
		// hive reaches back through the collection to teleport an action to a node in one of the others.
		collection.addHive(hiveHandle);
		hive -> setHiveCollection(&collection);

		hive -> setToolBindingsFactory(toolBindingsFactoryHandle);

		// Set before this hive's surfaces are served, so that the first chat request cannot arrive without a
		// model behind it. The factory is passed in rather than read back off the hive, so the harness cannot
		// end up with no chat tools purely because of the order these two calls are made in.
		// Built per hive, as the tool bindings it is given report on one hive, which means the servers the
		// harness file names are contacted once for every hive loaded here.
		hive -> setAgenticHarness(_buildAgenticHarness(hiveHandle, toolBindingsFactoryHandle));

		for(std::string surfaceName : hive -> getSurfaceNames())
		{
			Handle<GraphHiveSceneSurface> surfaceHandle = hive -> getSceneSurface(surfaceName);
			GraphHiveSceneSurface* surface = surfaceHandle.getInstance();

			// A surface this map cannot draw, i.e. one that is not a scene surface. Left out of the index
			// rather than fatal, as the hive is still perfectly serviceable without it being viewable.
			if(!surface) continue;

			// Nested under the hive's own page, so that the index reads as one tree rather than a listing
			// pointing off somewhere else.
			std::string surfacePath = _INDEX_PATH + hive -> getName() + "/" + surfaceName + "/";

			GraphHiveSceneSurfaceWebglMap* webglMap =
				new GraphHiveSceneSurfaceWebglMap(httpServer, *surface, surfacePath);

			Handle<GraphHiveSceneSurfaceWebglMap> webglMapHandle(webglMap);

			webglMapHandles.push_back(webglMapHandle);

			webglMap -> setPollInterval(_WEBGL_POLL_INTERVAL_MS);

			// The map asks for MEDIUM by default, which the harness file assigns no model to.
			webglMap -> setChatCapability(_CHAT_CAPABILITY);

			collectionMap -> addSurfaceMap(hive -> getName(), *webglMap);
		}
	}

	if(webglMapHandles.empty())
	{
		std::cerr << "None of the hive definition files loaded produced a scene surface to serve." << std::endl;
		exit(1);
	}

	try
	{
		httpServer.start();
	}
	catch(DisplayException& exception)
	{
		std::cerr << "Could not start HTTP server on port " << httpServer.getPort() << " (error "
			<< exception.getError() << ")" << std::endl;
		exit(1);
	}

	std::cout << "Listening on http://localhost:" << httpServer.getPort() << _INDEX_PATH << std::endl;

	signal(SIGINT, _handleSigInt);

	// This wait must happen before the process can otherwise go idle, or scene population stalls; this is
	// unrelated to strobing, which each hive has already been driving on its own scheduler thread for both the
	// root node's emitter and the surface itself since the calls above registered them, and which stays a
	// no-op until the hive's animation is switched on -- the engine's button here -- regardless of when the
	// wait below finishes.
	// Only one map need be waited on, since only one can be the first to be opened and whichever it is ends
	// the wait for all of them. The first map is the one blocked on because a wait has to be made against
	// something; the check that ends the loop is made against every map, so opening any hive's surface
	// releases it within a poll of it landing.
	bool receivedFirstRequest = false;

	while(_running && !receivedFirstRequest)
	{
		for(Handle<GraphHiveSceneSurfaceWebglMap>& webglMapHandle : webglMapHandles)
		{
			if(webglMapHandle.getInstance() -> hasReceivedFirstRequest())
			{
				receivedFirstRequest = true;
				break;
			}
		}

		if(!receivedFirstRequest)
		{
			webglMapHandles[0].getInstance() -> waitForFirstRequest(_FIRST_REQUEST_WAIT_POLL_MS);
		}
	}

	// Nothing left to drive from this thread: each hive's scheduler strobes both its root node and its surface
	// on their own cadences, and each map picks up its surface's refreshed contents via the surface changed
	// event fired by populateEnd(). Just wait for SIGINT.
	while(_running) pause();

	httpServer.stop();

	collection.shutdown();

	return 0;
}
