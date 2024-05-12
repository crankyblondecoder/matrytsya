#ifndef GRAPH_ACTION_H
#define GRAPH_ACTION_H

#include "../util/RefCounted.hpp"

/** The initial energy an action is assigned. */
#define INITIAL_ENERGY 255

class GraphActionThreadPoolWorkUnit;
class GraphEdge;
class GraphNode;

/**
 * Base class of all actions that traverses the graph and invoke operations on a node, as per a pre-defined action
 * specific interface.
 * @note Ref counted and will self de-reference once the action is complete. An action is complete once it can no longer
 *       traverse any edges.
 */
class GraphAction : private RefCounted
{
	friend GraphActionThreadPoolWorkUnit;
	friend GraphEdge;
	friend GraphNode;

    public:

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

	protected:

		// This is ref counted.
		virtual ~GraphAction();

		/**
		 * Subclass hook to apply this action to a node.
		 * @note This is not required to be re-entrant.
		 */
		virtual void _apply(GraphNode*) = 0;

		/**
		 * Set the edge traversal flags.
		 * @sa graphEdgeFlagRegister.hpp
		 * @param flags Bitfield representing the traversal flags.
		 */
		void _setEdgeTraversalFlags(unsigned long flags);

		/**
		 * Action is complete, will no longer traverse edges, and will soon be deleted.
		 */
		virtual void _complete() = 0;

    private:

		/** Work only lock. */
        ThreadMutex _workLock;

		/** Whether the action has been started. */
		bool _started;

		/** The curent node this action is acting upon. */
		GraphNode* _boundNode;

		/**
		 * The bitwise AND of this and the edge's traversal flags determines if the edge can be traversed.
		 * See graphEdgeFlagRegister.hpp
		 */
		unsigned long __edgeTraversalFlags;

		/**
		 * The number of energy units this action currently contains.
		 * This is part of the mechanism that prevents infinite loops.
		 */
		int _energy;

		/**
		 * Start traversal of graph from the given node.
		 * @note This is not re-entrant.
		 * @param origin The starting point in the graph of the action. The action will NOT be applied to this node. This node
		 *        is immediately traversed.
		 */
		void __start(GraphNode* origin);

		/**
		 * Consume some of the energy this action contains.
		 */
		void __consumeEnergy(int energyAmount);

		/**
		 * Worker thread entry point.
		 * @note Will only be called by a single thread at a time. ie It is not re-entrant.
		 */
		void __work();

		/**
		 * Requested work allocation was unable to be provided.
		 * ie __work() was not called.
		 */
		void __abortWork();
};

#endif
