#ifndef GRAPH_EDGE_H
#define GRAPH_EDGE_H

#include "GraphNodeHandle.hpp"

/**
 * Edge that describes directed link to another node.
 * @note Edges are immutable.
 */
class GraphEdge final
{
    public:

		/**
		 * Create directed link to a graph node.
		 * @param toNode Node edge points to.
		 */
		GraphEdge(GraphNodeHandle& toNode);

		~GraphEdge();

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

	protected:


    private:

		/** Node this edge points to. */
        GraphNodeHandle* _toNode;
};

#endif
