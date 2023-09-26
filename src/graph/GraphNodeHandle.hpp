#ifndef GRAPH_NODE_HANDLE_H
#define GRAPH_NODE_HANDLE_H

#include "GraphNode.hpp"

/**
 * Handle to a graph node.
 * Guarantees that the graph node this references will be available.
 * @note This is single thread only.
 */
class GraphNodeHandle
{
    public:

		/**
		 * Construct a handle from the given node.
		 */
       GraphNodeHandle(GraphNode* node);

		virtual ~GraphNodeHandle();

		GraphNode* getNode();

		bool isValid();

    protected:

    private:

	GraphNode* _referencedNode;
};

#endif
