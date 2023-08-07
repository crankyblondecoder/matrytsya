#ifndef GRAPH_EXCEPTION_H
#define GRAPH_EXCEPTION_H

#include <string>

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
			/// The provided edge handle is not valid.
			INVALID_EDGE_HANDLE,
			/// The provided node handle is not valid.
			INVALID_NODE_HANDLE,
			/// Could not allocate node list page in memory.
			NODE_LIST_PAGE_BAD_ALLOC,
			/// Node list page is already full.
			NODE_LIST_PAGE_FULL,
			/// Node list page item not found.
			NODE_LIST_PAGE_ITEM_NOT_FOUND,
			/// Something shouldn't have happened. This indicates a logic error.
			NODE_LIST_PAGE_UNEXPECTED,
			/// The requested node was not found.
			NODE_NOT_FOUND
        };

        virtual ~GraphException(){}

        GraphException(Error error) : Exception(Exception::GRAPH), _error{error} {}

    private:

        Error _error;
};

#endif