#include "agent/AgenticHarness.hpp"
#include "agent/Model.hpp"
#include "agent_bindings/ModelToolBindingsFactory.hpp"
#include "display/DisplayException.hpp"
#include "display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "display/http/HttpServer.hpp"
#include "util/Handle.hpp"
#include "graph/GraphHive.hpp"
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

#include <signal.h>
#include <unistd.h>

namespace
{
	volatile sig_atomic_t _running = 1;

	void _handleSigInt(int)
	{
		_running = 0;
	}

	// Run from the build directory (see build/makefile's debug_test target), so this is relative to that.
	const char* const _HIVE_JSON_PATH = "../examples/engineHive.json";
	//const char* const _HIVE_JSON_PATH = "../examples/flowerHive.json";
	//const char* const _HIVE_JSON_PATH = "../examples/roseHive.json";

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

	JsonHiveLoader loader(_readFile(_HIVE_JSON_PATH));

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphHiveSceneSurface> surfaceHandle = hive -> getDefaultSceneSurface();
	GraphHiveSceneSurface* surface = surfaceHandle.getInstance();

	if(!surface)
	{
		std::cerr << "Could not find scene surface in hive definition file: " << _HIVE_JSON_PATH << std::endl;
		exit(1);
	}

	// Built here rather than by the hive itself, as the concrete factory belongs to agent_bindings, which
	// depends on graph.
	ModelToolBindingsFactory* toolBindingsFactory = new ModelToolBindingsFactory();

	Handle<GraphToolBindingsFactory> toolBindingsFactoryHandle(toolBindingsFactory);

	// The handle holds the reference; release the implicit construction ref.
	toolBindingsFactory -> decrRef();

	hive -> setToolBindingsFactory(toolBindingsFactoryHandle);

	// Set before the surface is served, so that the first chat request cannot arrive without a model behind
	// it. The factory is passed in rather than read back off the hive, so the harness cannot end up with no
	// chat tools purely because of the order these two calls are made in.
	hive -> setAgenticHarness(_buildAgenticHarness(hiveHandle, toolBindingsFactoryHandle));

	HttpServer httpServer(8080);

	GraphHiveSceneSurfaceWebglMap* webglMap = new GraphHiveSceneSurfaceWebglMap(httpServer, *surface, "/scene/");
	Handle<GraphHiveSceneSurfaceWebglMap> webglMapHandle(webglMap);

	webglMap -> setPollInterval(_WEBGL_POLL_INTERVAL_MS);

	// The map asks for MEDIUM by default, which the harness file assigns no model to.
	webglMap -> setChatCapability(_CHAT_CAPABILITY);

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

	std::cout << "Listening on http://localhost:" << httpServer.getPort() << "/scene/" << std::endl;

	signal(SIGINT, _handleSigInt);

	// This wait must happen before the process can otherwise go idle, or scene population stalls; this is
	// unrelated to strobing, which the hive has already been driving on its own scheduler thread for both the
	// root node's emitter and the surface itself since the calls above registered them, and which stays a
	// no-op until the hive's animation is switched on -- the engine's button here -- regardless of when the
	// wait below finishes.
	while(_running && !webglMap -> hasReceivedFirstRequest())
	{
		webglMap -> waitForFirstRequest(_FIRST_REQUEST_WAIT_POLL_MS);
	}

	// Nothing left to drive from this thread: the scheduler strobes both the root node and the surface on
	// their own cadences, and webglMap picks up the surface's refreshed contents via the surface changed event
	// fired by populateEnd(). Just wait for SIGINT.
	while(_running) pause();

	httpServer.stop();

	hive -> shutdown();

	return 0;
}
