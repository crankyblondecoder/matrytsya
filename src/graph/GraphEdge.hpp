#ifndef GRAPH_EDGE_H
#define GRAPH_EDGE_H

#include <atomic>

#include "../util/RefCounted.hpp"

template <typename T> class GraphHandle;
class GraphNode;

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
		GraphEdge(GraphHandle<GraphNode>& toNode);

		/**
		 * Whether this edge points to a node.
		 * @returns True if complete. False otherwise.
		 */
		bool isComplete();

		/**
		 * Traverse this edge.
		 * @returns A graph node handle or null if traversal is not possible.
		 */
		GraphHandle<GraphNode> traverse();

		/**
		 * Get the unique id of this edge.
		 */
		unsigned getId();

		/**
		 * Add an action flag to this edge.
		 * This determines if an action is allowed to traverse this edge.
		 * @note If no action flags are set, then all actions are allowed to traverse this edge.
		 * @param actionFlag Action flag from action flag register.
		 */
		void addActionFlag(unsigned long actionFlag);

		/**
		 * Determine whether this edge can be traversed based on its action flags.
		 * @param actionFlags Action flags to check against.
		 */
		bool canTraverse(unsigned long actionFlags);

	protected:

		// This is a requirement of being ref counted.
		~GraphEdge();

    private:

		/// Counter used to derive each edge's unique id.
		static std::atomic<unsigned> _nextId;

		/// Unique id of this edge.
		unsigned _id;

		/** Node this edge points to. */
        GraphHandle<GraphNode>* _toNode;

		/// Flags that determine whether an action can traverse this edge.
		std::atomic<unsigned long> _actionFlags{0};
};

#endif
