#ifndef GRAPH_EDGE_H
#define GRAPH_EDGE_H

#include "../util/RefCounted.hpp"

class GraphNodeHandle;

/**
 * Edge that describes directed link to another node.
 * @note Edges are immutable.
 */
class GraphEdge : public RefCounted
{
    public:

		/**
		 * Create directed link to a graph node.
		 * @param toNode Node edge points to.
		 */
		GraphEdge(GraphNodeHandle& toNode);

		/**
		 * Whether this edge points to a node.
		 * @returns True if complete. False otherwise.
		 */
		bool isComplete();

		/**
		 * Traverse this edge.
		 * @returns A graph node handle or null if traversal is not possible.
		 */
		GraphNodeHandle traverse();

		/**
		 * Called by the node that contains the edge to say it has been bound to that node.
		 */
		void boundToNode();

	protected:

		// This is a requirement of being ref counted.
		~GraphEdge();

    private:

		/** Node this edge points to. */
        GraphNodeHandle* _toNode;
};

#endif
