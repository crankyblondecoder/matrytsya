#ifndef PERSIST_EXCEPTION_H
#define PERSIST_EXCEPTION_H

#include "../util/Exception.hpp"

/**
 * Exception thrown by the persist module, covering the builders' structural validation of loaded hive
 * and harness data and the JSON loaders' parsing/validation of that data as JSON. JSON-specific errors
 * are prefixed JSON_ so they can be distinguished from the format-agnostic builder errors without a
 * second exception class.
 */
class PersistException : public Exception
{
    public:

        enum Error
        {
            UNKNOWN,

			// -- HiveBuilder errors --

			/// The hive name was empty, or longer than GRAPH_HIVE_NAME_MAX_LEN.
			INVALID_HIVE_NAME,
			/// The hive had no nodes; a hive must contain at least one.
			NO_NODES,
			/// A node's name was empty.
			INVALID_NODE_NAME,
			/// Two or more nodes shared the same name within one hive.
			DUPLICATE_NODE_NAME,
			/// An edge referenced a "toNodeName" node name that does not exist among this hive's nodes.
			EDGE_TARGET_NOT_FOUND,
			/// GraphNode::createEdge returned an invalid handle.
			EDGE_CREATE_FAILED,
			/// An edge's action flags contained a name not present in the action flag register.
			UNKNOWN_ACTION_FLAG,
			/// A strobe emitter or strobe surface registration's periodMs was 0.
			INVALID_STROBE_PERIOD,
			/// A strobe emitter registration referenced a node name that does not exist among this hive's nodes.
			STROBE_EMITTER_NOT_FOUND,
			/// A strobe emitter registration referenced a node that is not a StrobeEmitterNode subclass.
			STROBE_EMITTER_WRONG_TYPE,
			/// A surface's name was empty.
			INVALID_SURFACE_NAME,
			/// Two or more surfaces shared the same name within one hive.
			DUPLICATE_SURFACE_NAME,
			/// A surface referenced a node name that does not exist among this hive's nodes.
			SURFACE_NODE_NOT_FOUND,
			/// A surface referenced a node that exists but is not the concrete type that surface requires.
			SURFACE_NODE_WRONG_TYPE,
			/// A strobe surface registration referenced a surface name that does not exist among this hive's surfaces.
			STROBE_SURFACE_NOT_FOUND,
			/// A capability was not one of the names in AgenticHarness::Capability.
			UNKNOWN_AGENT_CAPABILITY,
			/// An agent node prompt's node type was not one of the names in GraphNode::Type.
			UNKNOWN_AGENT_PROMPT_NODE_TYPE,

			// -- HarnessBuilder errors --

			/// The harness had no model providers; a harness must have at least one.
			NO_MODEL_PROVIDERS,
			/// A model provider's name was empty.
			INVALID_PROVIDER_NAME,
			/// Two or more model providers shared the same name within one harness.
			DUPLICATE_PROVIDER_NAME,
			/// A model provider's URL was empty.
			INVALID_PROVIDER_URL,
			/// A model provider could not be reached, or would not report the models it serves.
			PROVIDER_CONNECTION_FAILED,
			/// The harness had no model assignments; a harness must have at least one.
			NO_MODEL_ASSIGNMENTS,
			/// A model assignment referenced a provider name that does not exist among this harness's providers.
			MODEL_PROVIDER_NOT_FOUND,
			/// A model assignment referenced a model name that its provider does not serve.
			MODEL_NOT_FOUND,
			/// A role was not one of the names in AgenticHarness::Role.
			UNKNOWN_AGENT_ROLE,
			/// A model assignment's temperature was outside the range a model request permits.
			INVALID_MODEL_TEMPERATURE,
			/// A system prompt assignment's prompt text was empty.
			EMPTY_SYSTEM_PROMPT,
			/// A tool binding assignment named no role/capability pair to make its tools available to.
			TOOL_BINDING_WITHOUT_ROLE,
			/// A tool binding assignment named a set of tools that the hive's tool bindings factory does not supply.
			UNKNOWN_TOOL_BINDING_SET,
			/// A harness was built against a hive that could not be referenced, or with no tool bindings factory.
			HARNESS_HIVE_NOT_AVAILABLE,

			// -- JsonHiveLoader errors --

			/// The supplied JSON text could not be parsed.
			JSON_PARSE_ERROR,
			/// The top level JSON value was not an object.
			JSON_ROOT_NOT_OBJECT,
			/// The top level "name" member was missing, not a string, or outside 1-128 characters.
			JSON_INVALID_NAME,
			/// The top level "nodes" member was missing or not an array.
			JSON_INVALID_NODES,
			/// A node's "type" string did not match any known concrete node type.
			UNKNOWN_NODE_TYPE,
			/// A node object was missing the required "name" member, or it was not a string.
			JSON_INVALID_NODE_BASE,
			/// A node's "pokeEnabled" member was present but not a boolean.
			JSON_INVALID_POKE_ENABLED,
			/// A node's "edges" member was present but not an array, or an edge object was malformed.
			JSON_INVALID_EDGES,
			/// A TeleportNode's "destination" member was missing, not an object, or missing required fields.
			JSON_INVALID_DESTINATION,
			/// A node's "vertexes" member was present but not an array, or a vertex object was malformed.
			JSON_INVALID_VERTEXES,
			/// A node's "transform" member was present but not a 16 element numeric array.
			JSON_INVALID_TRANSFORM,
			/// A script node was missing required "coreScript" or "pokeScript", or either was not a string.
			JSON_INVALID_SCRIPT_SOURCE,
			/// The top level "strobeEmitters" member was present but not an array, or an entry was malformed.
			JSON_INVALID_STROBE_EMITTERS,
			/// A surface's "type" string did not match any known concrete surface type.
			UNKNOWN_SURFACE_TYPE,
			/// The top level "surfaces" member was present but not an array, or a surface object was malformed.
			JSON_INVALID_SURFACES,
			/// The top level "strobeSurfaces" member was present but not an array, or an entry was malformed.
			JSON_INVALID_STROBE_SURFACES,
			/// A surface's "default" member was present but not a boolean.
			JSON_INVALID_SURFACE_DEFAULT,
			/// A surface's "initialFocusNodeName" member was present but not a string.
			JSON_INVALID_SURFACE_INITIAL_FOCUS_NODE_NAME,
			/// A surface's "focusViewportFraction" member was present but not a positive number.
			JSON_INVALID_SURFACE_FOCUS_VIEWPORT_FRACTION,
			/// An AgentNode's "capability" member was missing or not a string.
			JSON_INVALID_AGENT_CAPABILITY,
			/// An AgentNode's "prompts" member was missing, not an array, empty, or a prompt object was malformed.
			JSON_INVALID_AGENT_PROMPTS,
			/// An AgentNode's "autoTriggerAgentAction" member was present but not a boolean.
			JSON_INVALID_AGENT_AUTO_TRIGGER,
			/// An AgentNode's "serialiseEmittedActions" member was present but not a boolean.
			JSON_INVALID_AGENT_SERIALISE_ACTIONS,
			/// A TriggerNode's "emitTriggerOnPoke" member was present but not a boolean.
			JSON_INVALID_TRIGGER_EMIT_ON_POKE,

			// -- JsonHarnessLoader errors --

			/// The top level "providers" member was missing or not an array.
			JSON_INVALID_PROVIDERS,
			/// A provider's "type" string did not match any known concrete provider type.
			UNKNOWN_PROVIDER_TYPE,
			/// A provider object was missing the required "name" or "url" member, or either was not a string.
			JSON_INVALID_PROVIDER,
			/// The top level "modelAssignments" member was missing or not an array.
			JSON_INVALID_MODEL_ASSIGNMENTS,
			/// A "roleCapability" member was missing, not an object, or missing its "role"/"capability" strings.
			JSON_INVALID_ROLE_CAPABILITY,
			/// A model assignment's "model" member was missing, not an object, or missing its name strings.
			JSON_INVALID_MODEL_REFERENCE,
			/// A model assignment's "temperature" member was present but not a number.
			JSON_INVALID_MODEL_TEMPERATURE,
			/// The top level "systemPrompts" member was present but not an array, or an entry was malformed.
			JSON_INVALID_SYSTEM_PROMPTS,
			/// The top level "toolBindings" member was present but not an array, or an entry was malformed.
			JSON_INVALID_TOOL_BINDINGS
        };

        virtual ~PersistException(){}

        PersistException(Error error) : Exception(Exception::PERSIST), _error{error} {}

        Error getError() {return _error;}

    private:

        Error _error;
};

#endif
