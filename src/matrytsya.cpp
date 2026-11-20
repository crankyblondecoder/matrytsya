#include "agent/AgenticHarness.hpp"
#include "agent/Model.hpp"
#include "agent_bindings/ModelToolBindingsFactory.hpp"
#include "api/ApiAuthenticator.hpp"
#include "api/ApiController.hpp"
#include "api/ApiException.hpp"
#include "api/ApiHttpMap.hpp"
#include "api/HiveHost.hpp"
#include "display/DisplayException.hpp"
#include "display/GraphHiveCollectionHttpMap.hpp"
#include "display/GraphHiveSurfaceHtmlMap.hpp"
#include "display/http/HttpServer.hpp"
#include "util/Handle.hpp"
#include "graph/GraphException.hpp"
#include "graph/GraphHive.hpp"
#include "graph/GraphHiveCollection.hpp"
#include "graph/GraphNode.hpp"
#include "graph/GraphToolBindingsFactory.hpp"
#include "persist/HarnessBuilder.hpp"
#include "persist/HiveLoader.hpp"
#include "persist/PersistException.hpp"
#include "persist/json/JsonHarnessLoader.hpp"
#include "persist/json/JsonHiveLoader.hpp"
#include "persist/lmdb/LmdbHiveLoader.hpp"
#include "persist/lmdb/LmdbHiveStore.hpp"
#include "thread/ThreadException.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <arpa/inet.h>
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

	// Loopback only unless asked otherwise, as the web interface has no authentication: anyone who can reach
	// it can drive the hives and, with a harness, the models behind them.
	const char* const _DEFAULT_ADDRESS = "127.0.0.1";

	/// Address that listens on every interface, which is not itself somewhere a browser can be pointed.
	const char* const _ANY_ADDRESS = "0.0.0.0";

	const unsigned _DEFAULT_NUM_THREADS = 2;

	const AgenticHarness::Capability _DEFAULT_CHAT_CAPABILITY = AgenticHarness::Capability::LOW;

	/// Verbosity at which model requests and responses start being logged.
	const unsigned _VERBOSITY_MODEL_TRAFFIC = 1;

	/// Verbosity at which node pokes start being logged.
	const unsigned _VERBOSITY_NODE_POKES = 2;

	const unsigned _WEBGL_POLL_INTERVAL_MS = 50;

	const GraphHiveSurfaceHtmlMap::ShadingModel _DEFAULT_SHADING_MODEL =
		GraphHiveSurfaceHtmlMap::ShadingModel::BLINN_PHONG;

	// Kept short rather than matched to how often a first request is actually expected, since this is also
	// how soon a SIGINT lands: waitForFirstRequest() runs a whole call out to completion once started, and
	// only the loop calling it re-checks _running in between.
	const unsigned _FIRST_REQUEST_WAIT_POLL_MS = 100;

	/// Path the collection index is mounted at, which is what a browser arriving at the server lands on.
	const char* const _INDEX_PATH = "/";

	/// Path the hive control API is mounted at. Its versioned base path is below this.
	const char* const _API_PATH = "/api";

	/// Values getopt_long gives the options that only have a long form.
	const int _OPTION_TLS_CERT = 256;
	const int _OPTION_TLS_KEY = 257;

	/**
	 * Print the command line synopsis and options.
	 * @param out Stream to print to.
	 * @param programName Name the program was invoked as.
	 */
	void _printUsage(std::ostream& out, const char* programName)
	{
		out << "Usage: " << programName << " [options] hive.json [hive.json ...]\n"
			"       " << programName << " [options] -d FILE [hive.json ...]\n"
			"\n"
			"Serve one or more hives, each defined by a JSON hive file or stored in a hive database, through a web\n"
			"interface.\n"
			"\n"
			"Options:\n"
			"  -d, --database FILE          Hive database to load hives from and keep them stored in, created if it\n"
			"                               does not exist. A hive already stored there is loaded from it in place\n"
			"                               of a hive file of the same name. Changes to a hive's structure while it\n"
			"                               runs are written back to it.\n"
			"  -p, --port PORT              Port the web interface listens on (default " << _DEFAULT_PORT << ").\n"
			"  -a, --address ADDRESS        IPv4 address of the interface the web interface listens on\n"
			"                               (default " << _DEFAULT_ADDRESS << "). Use " << _ANY_ADDRESS
			<< " for every interface.\n"
			"  -H, --harness FILE           Agentic harness definition to run the hives against. Without one\n"
			"                               the hives have no model behind them.\n"
			"  -c, --chat-capability LEVEL  Capability the chat window asks for: LOW, MEDIUM or HIGH\n"
			"                               (default " << AgenticHarness::getCapabilityName(_DEFAULT_CHAT_CAPABILITY)
			<< ").\n"
			"  -n, --no-chat                Do not display the chat window, and refuse chat requests.\n"
			"  -s, --shading MODEL          Lighting model the WebGL views shade with: BLINN_PHONG or LAMBERT\n"
			"                               (default " << GraphHiveSurfaceHtmlMap::getShadingModelName(
			_DEFAULT_SHADING_MODEL) << ").\n"
			"  -t, --threads COUNT          Threads in each hive's thread pool (default " << _DEFAULT_NUM_THREADS
			<< ").\n"
			"  -k, --api-token-env VAR      Enable the hive control API at " << _API_PATH << "/v1/, accepting the\n"
			"                               API token(s) held in environment variable VAR (comma separated, each\n"
			"                               at least " << ApiAuthenticator::MIN_TOKEN_LENGTH << " characters). Unless the address is loopback,\n"
			"                               --tls-cert and --tls-key are required. With the API enabled, the\n"
			"                               server may start with no hives.\n"
			"      --tls-cert FILE          PEM certificate (chain) to serve HTTPS with. Needs --tls-key.\n"
			"      --tls-key FILE           PEM private key of the certificate given by --tls-cert.\n"
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
	 * Test whether a command line argument is a dotted quad IPv4 address.
	 * @param text Argument to test.
	 * @returns True if the argument is an IPv4 address.
	 */
	bool _isIpv4Address(const char* text)
	{
		struct in_addr parsed;

		return inet_pton(AF_INET, text, &parsed) == 1;
	}

	/**
	 * Test whether an IPv4 address is a loopback address, which only this machine can reach.
	 * @param text Address to test, already known to be an IPv4 address.
	 */
	bool _isLoopbackAddress(const std::string& text)
	{
		struct in_addr parsed;

		if(inet_pton(AF_INET, text.c_str(), &parsed) != 1) return false;

		// 127.0.0.0/8, held in network byte order.
		return (ntohl(parsed.s_addr) >> 24) == 127;
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
	 * Parse a shading model from its name.
	 * @param name Name as given on the command line.
	 * @param shadingModel Set to the named shading model on success.
	 * @returns True if the name is one of the shading models.
	 */
	bool _parseShadingModel(const std::string& name, GraphHiveSurfaceHtmlMap::ShadingModel& shadingModel)
	{
		// Matched against the map's own names, so the ones accepted here cannot drift from the ones the page
		// is given.
		for(GraphHiveSurfaceHtmlMap::ShadingModel candidate : {GraphHiveSurfaceHtmlMap::ShadingModel::BLINN_PHONG,
			GraphHiveSurfaceHtmlMap::ShadingModel::LAMBERT})
		{
			if(GraphHiveSurfaceHtmlMap::getShadingModelName(candidate) == name)
			{
				shadingModel = candidate;

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

	/**
	 * Somewhere a hive is loaded from, as gathered before any hive is built.
	 */
	struct HiveSource
	{
		/// Loader holding the hive's definition, already read and validated as far as the format goes.
		std::unique_ptr<HiveLoader> loader;

		/// What the hive was loaded from, worded to follow "the hive " in a message.
		std::string description;

		/// Whether the hive came from the hive database rather than a hive file.
		bool fromStore;
	};

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
	std::string address = _DEFAULT_ADDRESS;
	unsigned numThreads = _DEFAULT_NUM_THREADS;
	AgenticHarness::Capability chatCapability = _DEFAULT_CHAT_CAPABILITY;
	bool chatEnabled = true;
	GraphHiveSurfaceHtmlMap::ShadingModel shadingModel = _DEFAULT_SHADING_MODEL;
	unsigned verbosity = 0;
	std::string harnessPath;
	std::string databasePath;
	std::string apiTokenEnv;
	std::string tlsCertPath;
	std::string tlsKeyPath;

	const struct option longOptions[] =
	{
		{"database", required_argument, 0, 'd'},
		{"port", required_argument, 0, 'p'},
		{"address", required_argument, 0, 'a'},
		{"harness", required_argument, 0, 'H'},
		{"chat-capability", required_argument, 0, 'c'},
		{"no-chat", no_argument, 0, 'n'},
		{"shading", required_argument, 0, 's'},
		{"threads", required_argument, 0, 't'},
		{"api-token-env", required_argument, 0, 'k'},
		{"tls-cert", required_argument, 0, _OPTION_TLS_CERT},
		{"tls-key", required_argument, 0, _OPTION_TLS_KEY},
		{"verbose", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	int option;

	while((option = getopt_long(argc, argv, "d:p:a:H:c:ns:t:k:vh", longOptions, 0)) != -1)
	{
		switch(option)
		{
			case 'd':

				databasePath = optarg;

				break;

			case 'p':

				if(!_parseUnsigned(optarg, 1, 65535, port))
				{
					std::cerr << argv[0] << ": invalid port '" << optarg << "', expected 1 to 65535" << std::endl;
					return 1;
				}

				break;

			case 'a':

				// Checked here rather than left to the server, which would only report that it could not start.
				if(!_isIpv4Address(optarg))
				{
					std::cerr << argv[0] << ": invalid address '" << optarg << "', expected an IPv4 address such as "
						<< _ANY_ADDRESS << std::endl;
					return 1;
				}

				address = optarg;

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

			case 's':

				if(!_parseShadingModel(optarg, shadingModel))
				{
					std::cerr << argv[0] << ": invalid shading model '" << optarg
						<< "', expected BLINN_PHONG or LAMBERT" << std::endl;
					return 1;
				}

				break;

			case 't':

				if(!_parseUnsigned(optarg, 1, 1024, numThreads))
				{
					std::cerr << argv[0] << ": invalid thread count '" << optarg << "', expected 1 to 1024"
						<< std::endl;
					return 1;
				}

				break;

			case 'k':

				apiTokenEnv = optarg;

				break;

			case _OPTION_TLS_CERT:

				tlsCertPath = optarg;

				break;

			case _OPTION_TLS_KEY:

				tlsKeyPath = optarg;

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

	if(tlsCertPath.empty() != tlsKeyPath.empty())
	{
		std::cerr << argv[0] << ": --tls-cert and --tls-key must be given together" << std::endl;
		return 1;
	}

	bool apiEnabled = !apiTokenEnv.empty();

	// Built before anything else is loaded, so that a missing or weak token is reported straight away rather
	// than after every hive has been built.
	std::unique_ptr<ApiAuthenticator> apiAuthenticatorBuilt;

	if(apiEnabled)
	{
		// The token would be sent in the clear over plain HTTP, so that is only allowed where it cannot leave this
		// machine.
		if(tlsCertPath.empty() && !_isLoopbackAddress(address))
		{
			std::cerr << argv[0] << ": the hive control API needs --tls-cert and --tls-key when listening on "
				<< address << ", which is not a loopback address" << std::endl;
			return 1;
		}

		const char* tokens = getenv(apiTokenEnv.c_str());

		std::string tokenText = tokens ? tokens : "";

		// Removed from the environment once read, so that nothing this process starts, or any script it runs,
		// can read it from there.
		unsetenv(apiTokenEnv.c_str());

		try
		{
			apiAuthenticatorBuilt = std::make_unique<ApiAuthenticator>(tokenText);
		}
		catch(ApiException& exception)
		{
			std::cerr << argv[0] << ": cannot enable the hive control API from environment variable " << apiTokenEnv
				<< ": " << exception.getDetail() << std::endl;
			return 1;
		}

		if(tlsCertPath.empty())
		{
			std::cerr << "Warning: the hive control API is served over plain HTTP. This is only safe because "
				<< address << " is a loopback address." << std::endl;
		}
	}

	Model::_logToConsole = verbosity >= _VERBOSITY_MODEL_TRAFFIC;
	GraphNode::_logToConsole = verbosity >= _VERBOSITY_NODE_POKES;

	std::vector<std::string> hiveJsonPaths(argv + optind, argv + argc);

	// Held for as long as the program runs, as every hive stored in it is kept up to date with it.
	Handle<LmdbHiveStore> storeHandle(0);

	if(!databasePath.empty())
	{
		try
		{
			LmdbHiveStore* store = new LmdbHiveStore(databasePath);

			storeHandle = Handle<LmdbHiveStore>(store);

			// The handle holds the reference; release the implicit construction ref.
			store -> decrRef();
		}
		catch(PersistException& exception)
		{
			std::cerr << "Could not open hive database " << databasePath << ": " << _describe(exception) << std::endl;
			exit(1);
		}
	}

	// Every hive is read before any is built, so that a hive file naming a hive the database already holds can be
	// set aside for the stored one, which has grown since that file was written.
	std::vector<HiveSource> hiveSources;

	if(storeHandle.isValid())
	{
		try
		{
			for(const std::string& hiveName : storeHandle.getInstance() -> getHiveNames())
			{
				try
				{
					hiveSources.push_back(HiveSource{std::make_unique<LmdbHiveLoader>(*storeHandle.getInstance(),
						hiveName), "'" + hiveName + "' stored in " + databasePath, true});
				}
				catch(PersistException& exception)
				{
					std::cerr << "Could not load the hive '" << hiveName << "' stored in " << databasePath << ": "
						<< _describe(exception) << std::endl;
					exit(1);
				}
			}
		}
		catch(PersistException& exception)
		{
			std::cerr << "Could not read hive database " << databasePath << ": " << _describe(exception) << std::endl;
			exit(1);
		}
	}

	for(const std::string& hiveJsonPath : hiveJsonPaths)
	{
		std::unique_ptr<HiveLoader> loader;

		try
		{
			loader = std::make_unique<JsonHiveLoader>(_readFile(hiveJsonPath));
		}
		catch(PersistException& exception)
		{
			std::cerr << "Could not load the hive described by " << hiveJsonPath << ": " << _describe(exception)
				<< std::endl;
			exit(1);
		}

		bool stored = false;

		for(HiveSource& source : hiveSources)
		{
			if(source.fromStore && source.loader -> getHiveName() == loader -> getHiveName()) stored = true;
		}

		if(stored)
		{
			std::cout << "Using the hive '" << loader -> getHiveName() << "' stored in " << databasePath
				<< " rather than the one described by " << hiveJsonPath << "." << std::endl;

			continue;
		}

		hiveSources.push_back(HiveSource{std::move(loader), "described by " + hiveJsonPath, false});
	}

	if(hiveSources.empty() && !apiEnabled)
	{
		if(storeHandle.isValid())
		{
			std::cerr << argv[0] << ": no hive definition files given, and " << databasePath << " holds no hives"
				<< std::endl;
		}
		else
		{
			std::cerr << argv[0] << ": no hive definition files given" << std::endl;
		}

		_printUsage(std::cerr, argv[0]);
		return 1;
	}

	// Declared before anything that points back at it: every hive loaded below is given this collection, and
	// so is the index map, neither of which reference count it.
	GraphHiveCollection collection;

	HttpServer httpServer(address, port);

	if(!tlsCertPath.empty())
	{
		try
		{
			httpServer.setTls(tlsCertPath, tlsKeyPath);
		}
		catch(DisplayException& exception)
		{
			std::cerr << "Could not read the TLS certificate " << tlsCertPath << " or its key " << tlsKeyPath
				<< std::endl;
			exit(1);
		}
	}

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

	HiveHost::Settings hostSettings;

	hostSettings.numThreads = numThreads;
	hostSettings.indexPath = _INDEX_PATH;
	hostSettings.pollIntervalMs = _WEBGL_POLL_INTERVAL_MS;
	hostSettings.shadingModel = shadingModel;
	hostSettings.chatEnabled = chatEnabled;
	hostSettings.chatCapability = chatCapability;

	// Built per hive, as the tool bindings it is given report on one hive, which means the servers the harness
	// file names are contacted once for every hive loaded, including any created through the control API.
	HiveHost::HarnessFactory harnessFactory = [&harnessPath, &toolBindingsFactoryHandle](Handle<GraphHive> hive)
	{
		return _buildAgenticHarness(harnessPath, hive, toolBindingsFactoryHandle);
	};

	HiveHost host(collection, httpServer, *collectionMap, storeHandle, toolBindingsFactoryHandle, harnessFactory,
		hostSettings);

	std::vector<std::string> hostedNames;

	for(HiveSource& source : hiveSources)
	{
		// Caught per hive, unlike in the test program, so that a bad hive among several is named.
		try
		{
			Handle<GraphHive> hive = host.addHive(*source.loader, source.fromStore);

			hostedNames.push_back(hive.getInstance() -> getName());
		}
		catch(PersistException& exception)
		{
			std::cerr << "Could not load the hive " << source.description << ": " << _describe(exception) << std::endl;
			exit(1);
		}
		catch(GraphException& exception)
		{
			if(exception.getError() == GraphException::DUPLICATE_HIVE)
			{
				std::cerr << "The hive " << source.description << " is named '" << source.loader -> getHiveName()
					<< "', which another hive already loaded is named too." << std::endl;
			}
			else
			{
				std::cerr << "Could not add the hive " << source.description << " (error "
					<< exception.getError() << ")" << std::endl;
			}

			exit(1);
		}
		catch(ThreadException& exception)
		{
			std::cerr << "Could not start keeping the hive " << source.description << " stored in "
				<< databasePath << std::endl;
			exit(1);
		}
	}

	unsigned surfaceCount = 0;

	for(std::string& hiveName : hostedNames) surfaceCount += host.getSurfacePaths(hiveName).size();

	if(surfaceCount == 0 && !apiEnabled)
	{
		std::cerr << "None of the hive definition files loaded produced a surface to serve." << std::endl;
		exit(1);
	}

	// With only one hive there is nothing to choose between at the index, so a browser arriving there is sent
	// straight to its view: the first scene surface, which links on to the structure page, or else its only
	// surface, or else the page listing its surfaces.
	if(hostedNames.size() == 1)
	{
		std::string firstScenePath = host.getFirstScenePath(hostedNames[0]);
		std::vector<std::pair<std::string, std::string>> surfacePaths = host.getSurfacePaths(hostedNames[0]);

		if(!firstScenePath.empty())
		{
			collectionMap -> setLandingPath(firstScenePath);
		}
		else if(surfacePaths.size() == 1)
		{
			collectionMap -> setLandingPath(surfacePaths[0].second);
		}
		else
		{
			collectionMap -> setLandingPath(_INDEX_PATH + hostedNames[0] + "/");
		}
	}

	// Only present when the API is enabled. Each refers to the next, so they are declared in that order and
	// released in the reverse.
	std::unique_ptr<ApiAuthenticator> apiAuthenticator;
	std::unique_ptr<ApiController> apiController;
	Handle<ApiHttpMap> apiMapHandle(0);

	if(apiEnabled)
	{
		apiAuthenticator = std::move(apiAuthenticatorBuilt);
		apiController = std::make_unique<ApiController>(host, collection);

		ApiHttpMap* apiMap = new ApiHttpMap(httpServer, *apiAuthenticator, *apiController, _API_PATH);

		apiMapHandle = Handle<ApiHttpMap>(apiMap);

		// The handle holds the reference; release the implicit construction ref, so that the map is unmounted
		// when the handle goes.
		apiMap -> decrRef();
	}

	try
	{
		httpServer.start();
	}
	catch(DisplayException& exception)
	{
		std::cerr << "Could not start HTTP server on " << httpServer.getAddress() << ":" << httpServer.getPort()
			<< " (error " << exception.getError() << ")" << std::endl;
		exit(1);
	}

	std::string scheme = httpServer.getTlsEnabled() ? "https" : "http";

	if(address == _ANY_ADDRESS)
	{
		std::cout << "Listening on every interface, port " << httpServer.getPort() << ", e.g. " << scheme
			<< "://localhost:" << httpServer.getPort() << _INDEX_PATH << std::endl;
	}
	else
	{
		std::cout << "Listening on " << scheme << "://" << address << ":" << httpServer.getPort() << _INDEX_PATH
			<< std::endl;
	}

	if(apiEnabled)
	{
		std::cout << "Hive control API at " << scheme << "://" << (address == _ANY_ADDRESS ? "localhost" : address)
			<< ":" << httpServer.getPort() << _API_PATH << "/v1/" << std::endl;
	}

	signal(SIGINT, _handleSigInt);
	signal(SIGTERM, _handleSigInt);

	{
		// Held only for this wait, so that a hive the control API later deletes is not kept mounted by it.
		std::vector<Handle<GraphHiveSurfaceHtmlMap>> surfaceMapHandles = host.getSurfaceMaps();

		// Only one map need be waited on, since only one can be the first to be opened and whichever it is ends
		// the wait for all of them. The first map is the one blocked on because a wait has to be made against
		// something; the check that ends the loop is made against every map, so opening any hive's surface
		// releases it within a poll of it landing.
		bool receivedFirstRequest = surfaceMapHandles.empty();

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
	}

	// Nothing left to drive from this thread: each hive's scheduler strobes both its root node and its surface
	// on their own cadences, and each map picks up its surface's refreshed contents via the surface changed
	// event fired by populateEnd(). Just wait for a signal to stop.
	while(_running) pause();

	httpServer.stop();

	// Before the hives are shut down, as a hive that has been holds nothing left to store.
	host.shutdown();

	collection.shutdown();

	return 0;
}
