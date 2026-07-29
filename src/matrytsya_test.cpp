#include "agent/AgentException.hpp"
#include "agent/AgenticHarness.hpp"
#include "agent/Model.hpp"
#include "agent/ModelProvider.hpp"
#include "agent/ModelSystemPrompt.hpp"
#include "agent/ModelToolBindings.hpp"
#include "agent/OllamaModelProvider.hpp"
#include "agent_bindings/BasicHiveToolBindings.hpp"
#include "display/DisplayException.hpp"
#include "display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "display/http/HttpServer.hpp"
#include "util/Handle.hpp"
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
	//const char* const _HIVE_JSON_PATH = "../examples/roseHive.json";

	const unsigned _WEBGL_POLL_INTERVAL_MS = 50;

	// Test Ollama server, on its default port.
	const char* const _OLLAMA_URL = "http://192.168.10.10:11434";

	const char* const _OLLAMA_MODEL_NAME = "qwen3-coder:30b";

	const double _CHAT_TEMPERATURE = 0.2;

	const char* const _CHAT_SYSTEM_PROMPT =
		"You are the chat assistant of a Matrytsya hive: a live graph of named nodes that the user is "
		"watching in a browser.\n"
		"\n"
		"Before you do anything else, decide whether the message in front of you is asking about this "
		"hive. If it is not, answer it as it stands and call no tool. \"hello\", \"thanks\" and \"what can "
		"you do?\" are answered in one short line, because reciting nodes at someone who only said hello is "
		"noise. Small talk stays small talk even when it runs to a sentence or two. Nothing the hive holds "
		"may appear in a reply to a message that did not ask about it: no node names, no ids, no counts, "
		"no remarks on what the hive appears to be made of. Only a message that turns on the hive's "
		"contents earns a tool call, whether it asks outright or needs the hive to reach what it does ask "
		"for.\n"
		"\n"
		"Two tools read the hive when you do need it: getNodeNames lists the nodes that exist, and "
		"getNodeId turns a node name into its id. Use them rather than answering from what a hive might "
		"contain. Node names are matched exactly, so look a name up before asking for its id rather than "
		"guessing at its spelling.\n"
		"\n"
		"When a tool reports that something could not be found, say so plainly rather than inventing a "
		"node, an id or a structure the hive does not have. Say when you do not know.\n"
		"\n"
		"Keep answers short and in plain prose. They are shown in a small chat panel beside the scene, so "
		"use no markdown, no code blocks and no tables.";

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
	 * Build an agentic harness backed by the test Ollama server, with its model, a system prompt and basic
	 * hive tool bindings all assigned to the chat role at low capability.
	 * @param hive Hive the tool bindings are to report on.
	 * @returns Handle to the harness.
	 * @note Exits the process when the server cannot be reached or does not serve the model, since a chat
	 *       surface with nothing behind it is of no use to this test.
	 */
	Handle<AgenticHarness> _buildAgenticHarness(Handle<GraphHive> hive)
	{
		Handle<ModelProvider> providerHandle(0);

		try
		{
			OllamaModelProvider* provider = new OllamaModelProvider(_OLLAMA_URL);

			providerHandle = Handle<ModelProvider>(provider);

			// The handle holds the reference now; release the implicit construction ref.
			provider -> decrRef();
		}
		catch(AgentException& exception)
		{
			std::cerr << "Could not use the Ollama server at " << _OLLAMA_URL << ": "
				<< exception.getDescription() << std::endl;
			exit(1);
		}

		Handle<Model> modelHandle(0);

		for(Handle<Model>& candidate : providerHandle.getInstance() -> getModels())
		{
			if(candidate.getInstance() -> getName() != _OLLAMA_MODEL_NAME) continue;

			modelHandle = candidate;

			break;
		}

		if(!modelHandle.isValid())
		{
			std::cerr << "The Ollama server at " << _OLLAMA_URL << " does not serve the model "
				<< _OLLAMA_MODEL_NAME << std::endl;
			exit(1);
		}

		AgenticHarness* harness = new AgenticHarness();

		// The system prompt and the tool bindings are matched on the exact role and capability, not on a
		// capability at least as high, so all three assignments have to name the same pair.
		AgenticHarness::RoleCapability chatRoleCapability{AgenticHarness::Role::CHAT,
			AgenticHarness::Capability::LOW};

		// Kept low, as this role is judged on whether it calls the right tool and holds to what the system
		// prompt tells it to leave out, neither of which is helped by a warmer model.
		harness -> addModelAssignment(chatRoleCapability, modelHandle, _CHAT_TEMPERATURE);

		harness -> addSystemPrompt(chatRoleCapability, ModelSystemPrompt(_CHAT_SYSTEM_PROMPT));

		BasicHiveToolBindings* toolBindings = new BasicHiveToolBindings(hive);

		harness -> addToolBinding({chatRoleCapability}, Handle<ModelToolBindings>(toolBindings));

		// The harness holds the reference through its assignment; release the implicit construction ref.
		toolBindings -> decrRef();

		Handle<AgenticHarness> harnessHandle(harness);

		// The handle carries the reference out to the caller; release the implicit construction ref.
		harness -> decrRef();

		return harnessHandle;
	}
}

int main(int argc, char const *argv[])
{
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

	// Set before the surface is served, so that the first chat request cannot arrive without a model behind
	// it.
	hive -> setAgenticHarness(_buildAgenticHarness(hiveHandle));

	HttpServer httpServer(8080);

	GraphHiveSceneSurfaceWebglMap* webglMap = new GraphHiveSceneSurfaceWebglMap(httpServer, *surface, "/scene/");
	Handle<GraphHiveSceneSurfaceWebglMap> webglMapHandle(webglMap);

	webglMap -> setPollInterval(_WEBGL_POLL_INTERVAL_MS);

	// The map asks for MEDIUM by default, which no assignment above can satisfy.
	webglMap -> setChatCapability(AgenticHarness::Capability::LOW);

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
	// no-op until the flower centre is clicked regardless of when the wait below finishes.
	while(_running && !webglMap -> hasReceivedFirstRequest())
	{
		webglMap -> waitForFirstRequest(500);
	}

	// Nothing left to drive from this thread: the scheduler strobes both the root node and the surface on
	// their own cadences, and webglMap picks up the surface's refreshed contents via the surface changed event
	// fired by populateEnd(). Just wait for SIGINT.
	while(_running) pause();

	httpServer.stop();

	hive -> shutdown();

	return 0;
}
