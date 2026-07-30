#ifndef GRAPH_EXCEPTION_H
#define GRAPH_EXCEPTION_H

#include "../util/Exception.hpp"

/**
 * Create and invoke thread entry point.
 */
class GraphException : public Exception
{
    public:

        enum Error
        {
            UNKNOWN,
			/// Could not allocate a new edge in memory.
			EDGE_BAD_ALLOC,
			/// General out of memory.
			OUT_OF_MEMORY,
			/// General buffer overflow.
			OVERFLOW,
			/// Re-entry of function is not permitted.
			RE_ENTRY_NOT_PERMITTED,
			/// The provided edge handle is not valid.
			INVALID_EDGE_HANDLE,
			/// The provided node handle is not valid.
			INVALID_NODE_HANDLE,
			/// The provided hive handle is not valid.
			INVALID_HIVE_HANDLE,
			/// A duplicate hive was attempted to be added to a collection.
			DUPLICATE_HIVE,
			/// Could not allocate node list page in memory.
			NODE_LIST_PAGE_BAD_ALLOC,
			/// Node list page is already full.
			NODE_LIST_PAGE_FULL,
			/// Node list page item not found.
			NODE_LIST_PAGE_ITEM_NOT_FOUND,
			/// Couldn't increase reference count on node, which was unexpected.
			NODE_LIST_COULDNT_REF_INCR,
			/// Something shouldn't have happened. This indicates a logic error.
			NODE_LIST_PAGE_UNEXPECTED,
			/// The requested node was not found.
			NODE_NOT_FOUND,
			/// Operation should have been implemented.
			OPERATION_MISSING,
			/// A serialisable action type was not recognised by the factory.
			UNKNOWN_ACTION_TYPE,
			/// The attempted teleportation of an action failed.
			ACTION_TELEPORT_FAILED,
			/// Could not allocate a new isolated Lua state for a script node.
			SCRIPT_STATE_BAD_ALLOC,
			/// Could not reference the node a script session was requested against.
			SCRIPT_SESSION_NODE_UNAVAILABLE,
			/// Whether the request of a hive surface should not have happened.
			HIVE_SURFACE_BAD_REQUEST,
			/// Timed out waiting for a scene surface to be generated.
			SCENE_SURFACE_GENERATION_TIMED_OUT,
			/// An agentic request was made against a hive with no agentic harness set.
			AGENTIC_HARNESS_NOT_SET,
			/// The supplied id does not name a chat context of the hive surface it was given to.
			INVALID_CHAT_CONTEXT_ID
        };

        virtual ~GraphException(){}

        GraphException(Error error) : Exception(Exception::GRAPH), _error{error} {}

        Error getError() {return _error;}

    private:

        Error _error;
};

#endif
