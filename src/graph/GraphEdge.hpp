#ifndef GRAPH_EDGE_H
#define GRAPH_EDGE_H

#include <atomic>

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
		 * Get the unique id of this edge.
		 */
		unsigned getId();

	protected:

		// This is a requirement of being ref counted.
		~GraphEdge();

    private:

		/// Counter used to derive each edge's unique id.
		static std::atomic<unsigned> _nextId;

		/// Unique id of this edge.
		unsigned _id;

		/** Node this edge points to. */
        GraphNodeHandle* _toNode;
};

#endif
