#ifndef GRAPH_EDGE_H
#define GRAPH_EDGE_H

#include <atomic>
#include <string>
#include <vector>

#include "../util/RefCounted.hpp"

template <typename T> class Handle;
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
		 * @param actionFlags List of action flags to add to edge. These determine if action is allowed to traverse
		 *        this edge. Leave list empty for no traversal restriction.
		 */
		GraphEdge(Handle<GraphNode>& toNode, std::vector<unsigned long> actionFlags);

		/**
		 * Whether this edge points to a node.
		 * @returns True if complete. False otherwise.
		 */
		bool isComplete();

		/**
		 * Traverse this edge.
		 * @returns A graph node handle or null if traversal is not possible.
		 */
		Handle<GraphNode> traverse();

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
		 * Determine if this edge has any of the given action flags.
		 */
		bool hasAnyActionFlags(unsigned long actionFlags);

		/**
		 * Determine whether this edge can be traversed based on its action flags.
		 * @param actionFlags Action flags to check against.
		 */
		bool canTraverse(unsigned long actionFlags);

		/**
		 * Get the description of this edge.
		 * @returns Description, or an empty string if none has been set.
		 */
		std::string getDescription();

		/**
		 * Set the description of this edge.
		 * @param description Description text.
		 */
		void setDescription(std::string description);

	protected:

		// This is a requirement of being ref counted.
		~GraphEdge();

    private:

		/// Counter used to derive each edge's unique id.
		static std::atomic<unsigned> _nextId;

		/// Unique id of this edge.
		unsigned _id;

		/** Node this edge points to. */
        Handle<GraphNode>* _toNode;

		/// Flags that determine whether an action can traverse this edge.
		std::atomic<unsigned long> _actionFlags{0};

		/// Optional description of this edge. Empty if none has been set.
		std::string _description;
};

#endif
