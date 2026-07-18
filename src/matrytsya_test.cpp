#include "display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "display/http/HttpServer.hpp"
#include "graph/GraphHandle.hpp"
#include "graph/GraphHive.hpp"
#include "graph/GraphHiveSceneSurface.hpp"
#include "persist/HiveBuilder.hpp"
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
	const char* const _HIVE_JSON_PATH = "../examples/flowerHive.json";

	const unsigned _WEBGL_POLL_INTERVAL_MS = 50;

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
}

int main(int argc, char const *argv[])
{
	JsonHiveLoader loader(_readFile(_HIVE_JSON_PATH));

	GraphHive* hive = HiveBuilder::build(loader, 2);
	GraphHandle<GraphHive> hiveHandle(hive);

	GraphHandle<GraphHiveSceneSurface> surfaceHandle = hive -> getSceneSurface("surface");
	GraphHiveSceneSurface* surface = surfaceHandle.getInstance();

	HttpServer httpServer(8080);

	GraphHiveSceneSurfaceWebglMap webglMap(httpServer, *surface, "/scene/");

	webglMap.setPollInterval(_WEBGL_POLL_INTERVAL_MS);

	httpServer.start();

	std::cout << "Listening on http://localhost:" << httpServer.getPort() << "/scene/" << std::endl;
	std::cout << "Click the flower centre to toggle the petal open/close animation." << std::endl;

	signal(SIGINT, _handleSigInt);

	// This wait must happen before the process can otherwise go idle, or scene population stalls; this is
	// unrelated to strobing, which the hive has already been driving on its own scheduler thread for both the
	// root node's emitter and the surface itself since the calls above registered them, and which stays a
	// no-op until the flower centre is clicked regardless of when the wait below finishes.
	while(_running && !webglMap.hasReceivedFirstRequest())
	{
		webglMap.waitForFirstRequest(500);
	}

	// Nothing left to drive from this thread: the scheduler strobes both the root node and the surface on
	// their own cadences, and webglMap picks up the surface's refreshed contents via the surface changed event
	// fired by populateEnd(). Just wait for SIGINT.
	while(_running) pause();

	httpServer.stop();

	hive -> shutdown();

	return 0;
}
