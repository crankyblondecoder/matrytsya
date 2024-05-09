#ifndef GRAPH_NODE_HANDLE_H
#define GRAPH_NODE_HANDLE_H

#include "GraphNode.hpp"

/**
 * Handle to a graph node.
 * Guarantees that the graph node this references will be available.
 * @note This is single thread only but is immutable so it shouldn't be a problem.
 */
class GraphNodeHandle
{
    public:

		/**
		 * Construct a handle from the given node.
		 */
       GraphNodeHandle(GraphNode* node);

		virtual ~GraphNodeHandle();

		/**
		 * Get the graph node contained in this handle.
		 * Will ref incr node before returning it.
		 * @returns Point to node. Null if not available.
		 */
		GraphNode* getNode();

		/** Get whether the handle is valid. ie A valid node pointer is available. */
		bool isValid();

    protected:

    private:

		GraphNode* _referencedNode;
};

#endif
