#ifndef GRAPH_ACTION_H
#define GRAPH_ACTION_H

class GraphNode;

/**
 * Base class of all actions that traverses the graph and invoke operations on a node, as per a pre-defined action
 * specific interface.
 */
class GraphAction
{
    public:

        virtual ~GraphAction();

		GraphAction();

		/**
		 * Get flag that determines if this action is invoked on a node i.e. The node is processed.
		 * @returns Bit field flag from action flag register.
		 */
        virtual unsigned long getFlag() = 0;

		/**
		 * Get the edge traversal flags for this action.
		 * @returns Flag bitfield.
		 */
		unsigned long getEdgeTraversalFlags();

		/**
		 * Start traversal of graph from the given node.
		 * @param origin The starting point in the graph of the action. The action will NOT be applied to this node. This node
		 *        is immediately traversed.
		 */
		void start(GraphNode* origin);

	protected:

		virtual GraphAction* clone() = 0;

		/**
		 * Subclass hook to apply this action to a node.
		 */
		virtual void _apply(GraphNode*) = 0;

		/**
		 * Set the edge traversal flags.
		 * @sa graphEdgeFlagRegister.hpp
		 * @param flags Bitfield representing the traversal flags.
		 */
		void _setEdgeTraversalFlags(unsigned long flags);

    private:

		GraphNode* __boundNode;

		/// The bitwise AND of this and the edge's traversal flags determines if the edge can be traversed.
		/// See graphEdgeFlagRegister.hpp
		unsigned long __edgeTraversalFlags;

		/**
		 * The number of energy units this action currently contains.
		 * This is part of the mechanism that prevents infinite loops.
		 */
		unsigned __energy;
};

#endif
