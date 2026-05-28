#ifndef GRAPH_ACTION_H
#define GRAPH_ACTION_H

#include "../util/RefCounted.hpp"

class GraphNode;
class GraphNodeHandle;

/**
 * Base class of all actions that traverses the graph and invoke operations on a node, as per a pre-defined action
 * specific interface.
 * @note Ref counted and will self de-reference once the action is complete. An action is complete once it can no longer
 *       traverse any edges.
 */
class GraphAction : public RefCounted
{
    public:

		/**
		 * @param initNode Initial node the new action is bound to. This action will not be applied to this node.
		 * @param energy The energy that is assigned to the action.
		 */
		GraphAction(GraphNodeHandle& initNode, unsigned energy);

		/**
		 * Get flag that determines if this action is invoked on a node i.e. The node is processed.
		 * @returns Bit field flag from action flag register.
		 */
        virtual unsigned long getFlag() = 0;

		/**
		 * Start traversal of graph.
		 * @note This is not re-entrant.
		 */
		void start();

		/**
		 * Worker thread entry point.
		 * @note Will only be called by a single thread at a time. ie It is not re-entrant.
		 */
		void work();

		/**
		 * Requested work allocation was unable to be provided.
		 * ie __work() was not called.
		 */
		void abortWork();

		/**
		 * Get the curent energy level of this action.
		 */
		unsigned getEnergyLevel();

	protected:

		// This is ref counted.
		virtual ~GraphAction();

		/**
		 * Subclass hook to apply this action to a node.
		 * @note This is not required to be re-entrant.
		 */
		virtual void _apply(GraphNode* node) = 0;

		/**
		 * Action is complete, will no longer traverse edges, and will soon be deleted.
		 */
		virtual void _complete() = 0;

    private:

		/** Work only lock. */
        ThreadMutex _workLock;

		/** Whether the action has been started. */
		bool _started;

		/**
		 * Whether the initial traverse has occurred.
		 * This exists to stop the action from being applied to the initial bound node.
		 */
		bool _initTraverse;

		/** Whether the action has stopped traversing, i.e. it will no longer be applied to any nodes */
		bool _stopped;

		/** The curent node this action is associated with. */
		GraphNodeHandle* _boundNode;

		/**
		 * The number of energy units this action currently contains.
		 * This is part of the mechanism that prevents infinite loops.
		 */
		unsigned _energy;

		/**
		 * Apply this action to the currently bound node.
		 */
		void __apply(GraphNode* node);

		/**
		 * Action is complete.
		 */
		void __complete();

		/**
		 * Consume an amount of energy that this action has.
		 */
		void __consumeEnergy(unsigned amount);
};

#endif
