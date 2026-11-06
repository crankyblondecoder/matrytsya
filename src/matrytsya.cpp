#include "agent/AgenticHarness.hpp"
#include "agent/Model.hpp"
#include "agent_bindings/ModelToolBindingsFactory.hpp"
#include "display/DisplayException.hpp"
#include "display/GraphHiveCollectionHttpMap.hpp"
#include "display/GraphHiveGraphViewSurfaceWebglMap.hpp"
#include "display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "display/GraphHiveSurfaceHtmlMap.hpp"
#include "display/http/HttpServer.hpp"
#include "util/Handle.hpp"
#include "graph/GraphException.hpp"
#include "graph/GraphHive.hpp"
#include "graph/GraphHiveCollection.hpp"
#include "graph/GraphHiveGraphViewSurface.hpp"
#include "graph/GraphHiveSceneSurface.hpp"
#include "graph/GraphNode.hpp"
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

#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

// Full usage reference is doc/matrytsya.md. Keep it and _printUsage() in step with the options parsed here.

namespace
{
	volatile sig_atomic_t _running = 1;

	void _handleSigInt(int)
	{
		_running = 0;
	}

	const unsigned _DEFAULT_PORT = 8080;

	const unsigned _DEFAULT_NUM_THREADS = 2;

	const AgenticHarness::Capability _DEFAULT_CHAT_CAPABILITY = AgenticHarness::Capability::LOW;

	/// Verbosity at which model requests and responses start being logged.
	const unsigned _VERBOSITY_MODEL_TRAFFIC = 1;

	/// Verbosity at which node pokes start being logged.
	const unsigned _VERBOSITY_NODE_POKES = 2;

	const unsigned _WEBGL_POLL_INTERVAL_MS = 50;

	// Kept short rather than matched to how often a first request is actually expected, since this is also
	// how soon a SIGINT lands: waitForFirstRequest() runs a whole call out to completion once started, and
	// only the loop calling it re-checks _running in between.
	const unsigned _FIRST_REQUEST_WAIT_POLL_MS = 100;

	/// Path the collection index is mounted at, which is what a browser arriving at the server lands on.
	const char* const _INDEX_PATH = "/";

	/**
	 * Print the command line synopsis and options.
	 * @param out Stream to print to.
	 * @param programName Name the program was invoked as.
	 */
	void _printUsage(std::ostream& out, const char* programName)
	{
		out << "Usage: " << programName << " [options] hive.json [hive.json ...]\n"
			"\n"
			"Serve one or more hives, each defined by a JSON hive file, through a web interface.\n"
			"\n"
			"Options:\n"
			"  -p, --port PORT              Port the web interface listens on (default " << _DEFAULT_PORT << ").\n"
			"  -H, --harness FILE           Agentic harness definition to run the hives against. Without one\n"
			"                               the hives have no model behind them.\n"
			"  -c, --chat-capability LEVEL  Capability the chat window asks for: LOW, MEDIUM or HIGH\n"
			"                               (default " << AgenticHarness::getCapabilityName(_DEFAULT_CHAT_CAPABILITY)
			<< ").\n"
			"  -n, --no-chat                Do not display the chat window, and refuse chat requests.\n"
			"  -t, --threads COUNT          Threads in each hive's thread pool (default " << _DEFAULT_NUM_THREADS
			<< ").\n"
			"  -v, --verbose                Log model traffic to the console. Repeat (-vv) to also log\n"
			"                               node pokes.\n"
			"  -h, --help                   Print this help and exit.\n"
			"\n"
			"See doc/matrytsya.md for the full reference.\n";
	}

	/**
	 * Parse an unsigned number from a command line argument.
	 * @param text Argument to parse.
	 * @param min Smallest value accepted.
	 * @param max Largest value accepted.
	 * @param value Set to the parsed value on success.
	 * @returns True if the whole argument was a number within range.
	 */
	bool _parseUnsigned(const char* text, unsigned long min, unsigned long max, unsigned& value)
	{
		// strtoul would otherwise wrap a leading minus round to a huge value rather than reject it.
		if(text[0] == '\0' || text[0] == '-') return false;

		char* end = 0;
		unsigned long parsed = strtoul(text, &end, 10);

		if(*end != '\0' || parsed < min || parsed > max) return false;

		value = (unsigned) parsed;

		return true;
	}

	/**
	 * Parse a capability from its name.
	 * @param name Name as given on the command line.
	 * @param capability Set to the named capability on success.
	 * @returns True if the name is one of the capabilities.
	 */
	bool _parseCapability(const std::string& name, AgenticHarness::Capability& capability)
	{
		// Matched against the harness's own names, so the ones accepted here cannot drift from the ones it
		// reports.
		for(AgenticHarness::Capability candidate : {AgenticHarness::Capability::LOW,
			AgenticHarness::Capability::MEDIUM, AgenticHarness::Capability::HIGH})
		{
			if(AgenticHarness::getCapabilityName(candidate) == name)
			{
				capability = candidate;

				return true;
			}
		}

		return false;
	}

	/**
	 * Describe what went wrong loading a definition file, for reporting to whoever wrote it.
	 * @param exception Exception raised while loading.
	 * @returns Plain language description, followed by what it went wrong against when that is known.
	 */
	std::string _describe(PersistException& exception)
	{
		std::string description = PersistException::getErrorDescription(exception.getError());

		if(!exception.getDetail().empty()) description += " -- " + exception.getDetail();

		return description;
	}

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
	 * @param harnessPath Path of the harness definition file. Empty when none was asked for. Cleared when the
	 *        harness cannot be built, so that the hives after this one are not each held up retrying a server
	 *        that has already failed to answer, and the failure is reported once rather than once per hive.
	 * @param hive Hive the tool bindings are to report on.
	 * @param toolBindingsFactory Factory the tool bindings are taken from.
	 * @returns Handle to the harness, or an invalid handle when none was asked for, or when the file cannot be
	 *          read or anything it asks for cannot be honoured -- an unreachable server included -- in which
	 *          case the hive is left with no model behind it.
	 * @note A failure here is reported and swallowed rather than fatal, unlike a hive that will not load:
	 *       a hive is still served with no model behind it, and the server the harness file names is not
	 *       always up.
	 */
	Handle<AgenticHarness> _buildAgenticHarness(std::string& harnessPath, Handle<GraphHive> hive,
		Handle<GraphToolBindingsFactory> toolBindingsFactory)
	{
		if(harnessPath.empty()) return Handle<AgenticHarness>(0);

		std::ifstream file(harnessPath);

		if(!file.is_open())
		{
			std::cerr << "Could not open harness definition file: " << harnessPath
				<< " -- running every hive with no model behind it." << std::endl;

			harnessPath.clear();

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
			std::cerr << "Could not build the harness described by " << harnessPath << ": " << _describe(exception)
				<< std::endl << "Running every hive with no model behind it." << std::endl;

			harnessPath.clear();

			return Handle<AgenticHarness>(0);
		}
	}
}

int main(int argc, char* argv[])
{
	unsigned port = _DEFAULT_PORT;
	unsigned numThreads = _DEFAULT_NUM_THREADS;
	AgenticHarness::Capability chatCapability = _DEFAULT_CHAT_CAPABILITY;
	bool chatEnabled = true;
	unsigned verbosity = 0;
	std::string harnessPath;

	const struct option longOptions[] =
	{
		{"port", required_argument, 0, 'p'},
		{"harness", required_argument, 0, 'H'},
		{"chat-capability", required_argument, 0, 'c'},
		{"no-chat", no_argument, 0, 'n'},
		{"threads", required_argument, 0, 't'},
		{"verbose", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	int option;

	while((option = getopt_long(argc, argv, "p:H:c:nt:vh", longOptions, 0)) != -1)
	{
		switch(option)
		{
			case 'p':

				if(!_parseUnsigned(optarg, 1, 65535, port))
				{
					std::cerr << argv[0] << ": invalid port '" << optarg << "', expected 1 to 65535" << std::endl;
					return 1;
				}

				break;

			case 'H':

				harnessPath = optarg;

				break;

			case 'c':

				if(!_parseCapability(optarg, chatCapability))
				{
					std::cerr << argv[0] << ": invalid chat capability '" << optarg
						<< "', expected LOW, MEDIUM or HIGH" << std::endl;
					return 1;
				}

				break;

			case 'n':

				chatEnabled = false;

				break;

			case 't':

				if(!_parseUnsigned(optarg, 1, 1024, numThreads))
				{
					std::cerr << argv[0] << ": invalid thread count '" << optarg << "', expected 1 to 1024"
						<< std::endl;
					return 1;
				}

				break;

			case 'v':

				verbosity++;

				break;

			case 'h':

				_printUsage(std::cout, argv[0]);
				return 0;

			default:

				// getopt_long has already said what was wrong with the option.
				_printUsage(std::cerr, argv[0]);
				return 1;
		}
	}

	Model::_logToConsole = verbosity >= _VERBOSITY_MODEL_TRAFFIC;
	GraphNode::_logToConsole = verbosity >= _VERBOSITY_NODE_POKES;

	if(optind >= argc)
	{
		std::cerr << argv[0] << ": no hive definition files given" << std::endl;
		_printUsage(std::cerr, argv[0]);
		return 1;
	}

	std::vector<std::string> hiveJsonPaths(argv + optind, argv + argc);

	// Declared before anything that points back at it: every hive loaded below is given this collection, and
	// so is the index map, neither of which reference count it.
	GraphHiveCollection collection;

	HttpServer httpServer(port);

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

	// Held for as long as the program runs, so that neither a hive nor a map it serves is released while the
	// server is still answering for it.
	std::vector<Handle<GraphHive>> hiveHandles;

	// Held as the HTML maps they are rather than as what each draws, since what is done with them from here on
	// -- setting them up, mounting them on the index and waiting on a first request -- is the same whichever
	// kind of surface is behind them.
	std::vector<Handle<GraphHiveSurfaceHtmlMap>> surfaceMapHandles;

	// Path of the first scene surface served, which is the one a lone hive is landed on.
	std::string firstScenePath;

	for(const std::string& hiveJsonPath : hiveJsonPaths)
	{
		GraphHive* hive = 0;

		// Caught per file, unlike in the test program, so that a bad file among several is named.
		try
		{
			JsonHiveLoader loader(_readFile(hiveJsonPath));

			hive = HiveBuilder::build(loader, numThreads);
		}
		catch(PersistException& exception)
		{
			std::cerr << "Could not load the hive described by " << hiveJsonPath << ": " << _describe(exception)
				<< std::endl;
			exit(1);
		}

		Handle<GraphHive> hiveHandle(hive);

		hiveHandles.push_back(hiveHandle);

		// Both directions of the association are needed: the collection serves the hive up by name, and the
		// hive reaches back through the collection to teleport an action to a node in one of the others.
		try
		{
			collection.addHive(hiveHandle);
		}
		catch(GraphException& exception)
		{
			if(exception.getError() == GraphException::DUPLICATE_HIVE)
			{
				std::cerr << "The hive described by " << hiveJsonPath << " is named '" << hive -> getName()
					<< "', which another hive already loaded is named too." << std::endl;
			}
			else
			{
				std::cerr << "Could not add the hive described by " << hiveJsonPath << " (error "
					<< exception.getError() << ")" << std::endl;
			}

			exit(1);
		}

		hive -> setHiveCollection(&collection);

		hive -> setToolBindingsFactory(toolBindingsFactoryHandle);

		// Set before this hive's surfaces are served, so that the first chat request cannot arrive without a
		// model behind it. The factory is passed in rather than read back off the hive, so the harness cannot
		// end up with no chat tools purely because of the order these two calls are made in.
		// Built per hive, as the tool bindings it is given report on one hive, which means the servers the
		// harness file names are contacted once for every hive loaded here.
		hive -> setAgenticHarness(_buildAgenticHarness(harnessPath, hiveHandle, toolBindingsFactoryHandle));

		// Worked out before any map is built, since a scene surface has to be told where this hive's structure
		// is when its map is made, and the surface holding it may well be listed after it. Empty when the hive
		// mounts no graph view surface, which is what leaves a scene page offering no such link.
		std::string structurePath;

		for(std::string surfaceName : hive -> getSurfaceNames())
		{
			if(!hive -> getGraphViewSurface(surfaceName).isValid()) continue;

			structurePath = _INDEX_PATH + hive -> getName() + "/" + surfaceName + "/";

			break;
		}

		for(std::string surfaceName : hive -> getSurfaceNames())
		{
			// Nested under the hive's own page, so that the index reads as one tree rather than a listing
			// pointing off somewhere else.
			std::string surfacePath = _INDEX_PATH + hive -> getName() + "/" + surfaceName + "/";

			Handle<GraphHiveSceneSurface> sceneSurfaceHandle = hive -> getSceneSurface(surfaceName);
			Handle<GraphHiveGraphViewSurface> graphViewSurfaceHandle = hive -> getGraphViewSurface(surfaceName);

			GraphHiveSurfaceHtmlMap* surfaceMap = 0;

			if(sceneSurfaceHandle.isValid())
			{
				surfaceMap = new GraphHiveSceneSurfaceWebglMap(httpServer, *sceneSurfaceHandle.getInstance(),
					surfacePath);

				// Only a scene page is offered the link. The structure page is the one it would lead to.
				surfaceMap -> setStructurePath(structurePath);

				if(firstScenePath.empty()) firstScenePath = surfacePath;
			}
			else if(graphViewSurfaceHandle.isValid())
			{
				surfaceMap = new GraphHiveGraphViewSurfaceWebglMap(httpServer,
					*graphViewSurfaceHandle.getInstance(), surfacePath);
			}
			else
			{
				// A surface of a kind nothing here knows how to draw. Left out of the index rather than
				// fatal, as the hive is still perfectly serviceable without it being viewable.
				continue;
			}

			Handle<GraphHiveSurfaceHtmlMap> surfaceMapHandle(surfaceMap);

			surfaceMapHandles.push_back(surfaceMapHandle);

			surfaceMap -> setPollInterval(_WEBGL_POLL_INTERVAL_MS);

			surfaceMap -> setChatEnabled(chatEnabled);

			// The map asks for MEDIUM by default, which a harness need not assign any model to.
			surfaceMap -> setChatCapability(chatCapability);

			collectionMap -> addSurfaceMap(hive -> getName(), *surfaceMap);
		}
	}

	if(surfaceMapHandles.empty())
	{
		std::cerr << "None of the hive definition files loaded produced a surface to serve." << std::endl;
		exit(1);
	}

	// With only one hive there is nothing to choose between at the index, so a browser arriving there is sent
	// straight to its view: the first scene surface, which links on to the structure page, or else its only
	// surface, or else the page listing its surfaces.
	if(hiveHandles.size() == 1)
	{
		if(!firstScenePath.empty())
		{
			collectionMap -> setLandingPath(firstScenePath);
		}
		else if(surfaceMapHandles.size() == 1)
		{
			collectionMap -> setLandingPath(surfaceMapHandles[0].getInstance() -> getPath());
		}
		else
		{
			collectionMap -> setLandingPath(_INDEX_PATH + hiveHandles[0].getInstance() -> getName() + "/");
		}
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
	signal(SIGTERM, _handleSigInt);

	// This wait must happen before the process can otherwise go idle, or scene population stalls. Strobing is
	// unrelated: each hive has already been driving it on its own scheduler thread since the calls above
	// registered its emitters and surfaces.
	// Only one map need be waited on, since only one can be the first to be opened and whichever it is ends
	// the wait for all of them. The first map is the one blocked on because a wait has to be made against
	// something; the check that ends the loop is made against every map, so opening any hive's surface
	// releases it within a poll of it landing.
	bool receivedFirstRequest = false;

	while(_running && !receivedFirstRequest)
	{
		for(Handle<GraphHiveSurfaceHtmlMap>& surfaceMapHandle : surfaceMapHandles)
		{
			if(surfaceMapHandle.getInstance() -> hasReceivedFirstRequest())
			{
				receivedFirstRequest = true;
				break;
			}
		}

		if(!receivedFirstRequest)
		{
			surfaceMapHandles[0].getInstance() -> waitForFirstRequest(_FIRST_REQUEST_WAIT_POLL_MS);
		}
	}

	// Nothing left to drive from this thread: each hive's scheduler strobes both its root node and its surface
	// on their own cadences, and each map picks up its surface's refreshed contents via the surface changed
	// event fired by populateEnd(). Just wait for a signal to stop.
	while(_running) pause();

	httpServer.stop();

	collection.shutdown();

	return 0;
}
