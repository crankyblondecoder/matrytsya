#ifndef GRAPH_ACTION_H
#define GRAPH_ACTION_H

class GraphNode;

/**
 * Base class of all actions that traverses the graph and invoke operations on a node, as per a pre-defined action
 * specific interface.
 * This is a light weight mechanism because, practically, if an action had to understand the interface of
 * a large number of node types it would have to be updated to take into account every new node type added
 * to a graph that it wanted to act on. A node, however, will only be acted on by a small number of actions so
 * it is better for a node to hold the logic to act on a particular action.
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
		 * Apply this action to an action targetable instance.
		 * Only the subclass will know which specific GraphActionTarget to apply to.
		 */
		virtual void apply(GraphNode*) = 0;

		/**
		 * Get the edge traversal flags for this action.
		 * @returns Flag bitfield.
		 */
		unsigned long getEdgeTraversalFlags();

	protected:

		virtual GraphAction* clone() = 0;

		/**
		 * Set the edge traversal flags.
		 * @sa graphEdgeFlagRegister.hpp
		 * @param flags Bitfield representing the traversal flags.
		 */
		void _setEdgeTraversalFlags(unsigned long flags);

    private:

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
