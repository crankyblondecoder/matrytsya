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
			/// The provided edge handle is not valid.
			INVALID_EDGE_HANDLE,
			/// Could not allocate a new edge in memory.
			EDGE_BAD_ALLOC
        };

        virtual ~GraphException(){}

        GraphException(Error error) : Exception(Exception::GRAPH), _error{error} {}

    private:

        Error _error;
};

#endif