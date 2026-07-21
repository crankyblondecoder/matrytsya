#ifndef PERSIST_EXCEPTION_H
#define PERSIST_EXCEPTION_H

#include "../util/Exception.hpp"

/**
 * Exception thrown by the persist module, covering both HiveBuilder's structural validation of a
 * loaded hive and JsonHiveLoader's parsing/validation of JSON hive data. JSON-specific errors are
 * prefixed JSON_ so they can be distinguished from format-agnostic HiveBuilder errors without a
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
			/// A node's "notifySources" referenced a node name that does not exist among this hive's nodes.
			NOTIFY_SOURCE_NOT_FOUND,
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
			/// A node's "notifySources" member was present but not an array, or an entry was not a string.
			JSON_INVALID_NOTIFY_SOURCES,
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
			JSON_INVALID_SURFACE_FOCUS_VIEWPORT_FRACTION
        };

        virtual ~PersistException(){}

        PersistException(Error error) : Exception(Exception::PERSIST), _error{error} {}

        Error getError() {return _error;}

    private:

        Error _error;
};

#endif
