#include "PersistException.hpp"

// No default case, so that a new error left undescribed here is a compiler warning rather than a message that
// says nothing.
const char* PersistException::getErrorDescription(Error error)
{
	switch(error)
	{
		case UNKNOWN: break;

		// -- HiveBuilder errors --

		case INVALID_HIVE_NAME: return "the hive name is empty or too long";
		case NO_NODES: return "the hive has no nodes";
		case INVALID_NODE_NAME: return "a node has an empty name";
		case DUPLICATE_NODE_NAME: return "two or more nodes share the same name";
		case EDGE_TARGET_NOT_FOUND: return "an edge points to a node that does not exist";
		case EDGE_CREATE_FAILED: return "an edge could not be created";
		case UNKNOWN_ACTION_FLAG: return "an edge names an action flag that does not exist";
		case INVALID_STROBE_PERIOD: return "a strobe emitter or strobe surface has a period of 0";
		case STROBE_EMITTER_NOT_FOUND: return "a strobe emitter names a node that does not exist";
		case STROBE_EMITTER_WRONG_TYPE: return "a strobe emitter names a node that cannot emit strobes";
		case INVALID_SURFACE_NAME: return "a surface has an empty name";
		case DUPLICATE_SURFACE_NAME: return "two or more surfaces share the same name";
		case SURFACE_NODE_NOT_FOUND: return "a surface names a node that does not exist";
		case SURFACE_NODE_WRONG_TYPE: return "a surface names a node of the wrong type for that surface";
		case STROBE_SURFACE_NOT_FOUND: return "a strobe surface names a surface that does not exist";
		case UNKNOWN_AGENT_CAPABILITY: return "a capability is not LOW, MEDIUM or HIGH";
		case UNKNOWN_AGENT_PROMPT_NODE_TYPE: return "an agent node prompt names a node type that does not exist";
		case UNKNOWN_VERTEX_VISIBILITY: return "a vertex group has an unknown visibility";

		// -- HarnessBuilder errors --

		case NO_MODEL_PROVIDERS: return "the harness has no model providers";
		case INVALID_PROVIDER_NAME: return "a model provider has an empty name";
		case DUPLICATE_PROVIDER_NAME: return "two or more model providers share the same name";
		case INVALID_PROVIDER_URL: return "a model provider has an empty URL";
		case PROVIDER_CONNECTION_FAILED: return "could not get the list of models from a model provider";
		case MISSING_PROVIDER_API_KEY: return "the environment variable a model provider reads its API key from is not set";
		case NO_MODEL_ASSIGNMENTS: return "the harness has no model assignments";
		case MODEL_PROVIDER_NOT_FOUND: return "a model assignment names a model provider that does not exist";
		case MODEL_NOT_FOUND: return "a model assignment names a model its provider does not serve";
		case UNKNOWN_AGENT_ROLE: return "a role is not CHAT, HIVE, NODE or SCRIPT";
		case INVALID_MODEL_TEMPERATURE: return "a model assignment's temperature is out of range";
		case EMPTY_SYSTEM_PROMPT: return "a system prompt is empty";
		case TOOL_BINDING_WITHOUT_ROLE: return "a tool binding names no role and capability to apply to";
		case UNKNOWN_TOOL_BINDING_SET: return "a tool binding names a tool set that does not exist";
		case HARNESS_HIVE_NOT_AVAILABLE: return "the harness has no hive or tool bindings to be built against";

		// -- JsonHiveLoader errors --

		case JSON_PARSE_ERROR: return "the file is not valid JSON";
		case JSON_ROOT_NOT_OBJECT: return "the file's top level JSON value is not an object";
		case JSON_INVALID_NAME: return "\"name\" is missing, not a string, or not 1 to 128 characters long";
		case JSON_INVALID_NODES: return "\"nodes\" is missing or not an array";
		case UNKNOWN_NODE_TYPE: return "a node's \"type\" is not a known node type";
		case JSON_INVALID_NODE_BASE: return "a node's \"name\" is missing or not a string";
		case JSON_INVALID_POKE_ENABLED: return "a node's \"pokeEnabled\" is not a boolean";
		case JSON_INVALID_EDGES: return "a node's \"edges\" is not an array, or an edge in it is malformed";
		case JSON_INVALID_DESTINATION: return "a teleport node's \"destination\" is missing or malformed";
		case JSON_INVALID_VERTEXES: return "a node's \"vertexes\" is not an array, or a vertex in it is malformed";
		case JSON_INVALID_VERTEX_GROUPS:
			return "a node's \"vertexGroups\" is not an array, or a group in it is malformed";
		case JSON_VERTEXES_AND_VERTEX_GROUPS: return "a node has both \"vertexes\" and \"vertexGroups\"";
		case JSON_INVALID_EMIT_AGENT_AFFECT_ACTION: return "a node's \"emitAgentAffectAction\" is not a boolean";
		case JSON_INVALID_TRANSFORM: return "a node's \"transform\" is not an array of 16 numbers";
		case JSON_INVALID_SCRIPT_SOURCE:
			return "a script node's \"coreScript\" or \"pokeScript\" is missing or not a string";
		case JSON_INVALID_STROBE_EMITTERS: return "\"strobeEmitters\" is not an array, or an entry in it is malformed";
		case UNKNOWN_SURFACE_TYPE: return "a surface's \"type\" is not a known surface type";
		case JSON_INVALID_SURFACES: return "\"surfaces\" is not an array, or a surface in it is malformed";
		case JSON_INVALID_STROBE_SURFACES: return "\"strobeSurfaces\" is not an array, or an entry in it is malformed";
		case JSON_INVALID_SURFACE_DEFAULT: return "a surface's \"default\" is not a boolean";
		case JSON_INVALID_SURFACE_INITIAL_FOCUS_NODE_NAME:
			return "a surface's \"initialFocusNodeName\" is not a string";
		case JSON_INVALID_SURFACE_FOCUS_VIEWPORT_FRACTION:
			return "a surface's \"focusViewportFraction\" is not a number greater than 0 and no greater than 1";
		case JSON_INVALID_AGENT_CAPABILITY: return "an agent node's \"capability\" is missing or not a string";
		case JSON_INVALID_AGENT_PROMPTS:
			return "an agent node's \"prompts\" is missing, empty, not an array, or has a malformed prompt in it";
		case JSON_INVALID_AGENT_AUTO_TRIGGER: return "an agent node's \"autoTriggerAgentAction\" is not a boolean";
		case JSON_INVALID_AGENT_SERIALISE_ACTIONS:
			return "an agent node's \"serialiseEmittedActions\" is not a boolean";
		case JSON_INVALID_TRIGGER_EMIT_ON_POKE: return "a trigger node's \"emitTriggerOnPoke\" is not a boolean";
		case JSON_INVALID_TEXT: return "a scene text node's \"text\" is not a string";
		case JSON_INVALID_TEXT_POSITION: return "a scene text node's \"textPosition\" is not an array of 3 numbers";
		case JSON_INVALID_TEXT_HEIGHT: return "a scene text node's \"textHeight\" is not a number greater than 0";
		case JSON_INVALID_TEXT_COLOUR:
			return "a scene text node's \"textColour\" is not an array of 4 integers, each from 0 to 255";
		case JSON_INVALID_TEXT_BACKGROUND:
			return "a scene text node's \"textBackground\" is not an object, or a colour, length or corner radius in it "
				"is malformed or out of range";
		case JSON_INVALID_TEXT_VISIBLE: return "a scene text node's \"textVisible\" is not a boolean";

		// -- JsonHarnessLoader errors --

		case JSON_INVALID_PROVIDERS: return "\"providers\" is missing or not an array";
		case UNKNOWN_PROVIDER_TYPE: return "a provider's \"type\" is not a known provider type";
		case JSON_INVALID_PROVIDER: return "a provider is missing its \"name\", or a member its type requires (\"url\" for Ollama, \"apiKeyEnv\" for OpenRouter), or one of them is not a string";
		case JSON_INVALID_MODEL_ASSIGNMENTS: return "\"modelAssignments\" is missing or not an array";
		case JSON_INVALID_ROLE_CAPABILITY: return "a \"roleCapability\" is missing or malformed";
		case JSON_INVALID_MODEL_REFERENCE: return "a model assignment's \"model\" is missing or malformed";
		case JSON_INVALID_MODEL_TEMPERATURE: return "a model assignment's \"temperature\" is not a number";
		case JSON_INVALID_SYSTEM_PROMPTS: return "\"systemPrompts\" is not an array, or an entry in it is malformed";
		case JSON_INVALID_TOOL_BINDINGS: return "\"toolBindings\" is not an array, or an entry in it is malformed";
	}

	return "an unknown error occurred";
}
