#ifndef GRAPH_ACTION_H
#define GRAPH_ACTION_H

#include <atomic>
#include <vector>

#include "../util/RefCounted.hpp"
#include "../thread/ThreadCondition.hpp"
#include "GraphHandle.hpp"

class GraphEdge;
class GraphHive;
class GraphNode;

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
		GraphAction(GraphHandle<GraphNode> initNode, unsigned energy);

		/**
		 * Get the required flags that determine if this action is invoked on a node.
		 * All of these flags must be present on the node for it to be processed.
		 * @returns Bit field of flags from action flag register or'ed together.
		 */
        virtual unsigned long getRequiredFlags() final;

		/**
		 * Get the optional flags that determine if this action is invoked on a node.
		 * If no required flags are present, at least one of these flags must be present on a node for it to be
		 * processed.
		 * @returns Bit field of flags from action flag register or'ed together.
		 */
        virtual unsigned long getOptionalFlags() final;

		/**
		 * Get flags that apply to edge traversal.
		 */
		virtual unsigned long getEdgeTraversalFlags() final;

		/**
		 * Get the unique id of this action.
		 */
		unsigned getId();

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

		/**
		 * Wait on this action completing.
		 * @param timeOut Maximum period in ms to wait on condition to be signalled. Use 0 for no timeout.
		 */
		void waitOnComplete(unsigned timeOut);

		/**
		 * Get whether this action has completed, i.e. it will no longer traverse edges or be applied to any nodes.
		 */
		bool isComplete();

		/**
		 * Get whether this action can traverse the given edge.
		 * An action ultimately determines which pathway it takes through the graph.
		 * @note Standard behaviour is to not traverse an edge that has already been traversed.
		 */
		virtual bool canTraverseEdge(GraphHandle<GraphEdge> handle);

		/**
		 * If called, this action will be applied to the initial node.
		 * @note This _must_ be called prior to starting the action.
		 */
		void setApplyToInitialNode();

		/**
		 * Apply this action to the given node as a result of the node scheduling it.
		 * @note This should _only_ be used by a graph nodes action scheduling mechanism.
		 * @param nodeHandle Node to apply this action to.
		 */
		void applyScheduled(GraphHandle<GraphNode> nodeHandle);

	protected:

		// This is a requirement of being ref counted.
		virtual ~GraphAction();

		/**
		 * Add flag that determines if this action is invoked on a node i.e. The node is processed.
		 * @param flag Flag to add.
		 * @param required If true, the flag is required on the node for the action to be invoked on it.
		 */
		void _addFlag(unsigned long flag, bool required);

		/**
		 * Action is starting.
 		 * @note If this is called, it will always invoke _complete.
		 * @returns True if should continue action application. If false, immediately complete without apply.
		 */
		virtual bool _starting() = 0;

		/**
		 * Action is complete, will no longer traverse edges, and will soon be deleted.
		 */
		virtual void _complete() = 0;

		/**
		 * Apply this action to a node.
		 */
		virtual void _apply(GraphNode* node) = 0;

    private:

		/// Generic lock.
        ThreadMutex _lock;

		/// Counter used to derive each action's unique id.
		static std::atomic<unsigned> _nextId;

		/// Unique id of this action.
		unsigned _id;

		/// For any thread that wants to wait on the action completing.
		ThreadCondition _completeCond;

		/// Whether the action has been started.
		bool _started = false;

		/// Whether to apply to the initial node.
		bool _applyToInitNode = false;

		/**
		 * Whether the initial traverse has occurred.
		 * This exists to stop the action from being applied to the initial bound node if not required.
		 */
		bool _initTraverse = true;

		/**
		 * Whether the action has stopped traversing, i.e. it will no longer be applied to any nodes.
		 * If true, this indicates that this action is complete.
		 */
		bool _stopped = false;

		/// Handle to the curent node this action is associated with.
		GraphHandle<GraphNode> _boundNode;

		/// Hive this action is traversing. Should always be set to the hive of the initial node.
		GraphHandle<GraphHive> _boundHive;

		/**
		 * The number of energy units this action currently contains.
		 * This is part of the mechanism that prevents infinite loops.
		 */
		unsigned _energy;

		/// Handle supplied by the bound hive's actionActive(), used to notify actionInactive() on completion.
		unsigned _hiveActionHandle = 0;

		/// Whether this action is currently registered as active with the bound hive.
		bool _hiveActionRegistered = false;

		/**
		 * Required flags that determine if this action is invoked on a node.
		 * The node must have all these flags.
		 */
		std::atomic<unsigned long> _requiredFlags{0};

		/**
		 * Optional flags that determine if this action is invoked on a node.
		 * These are only used if no required flags are present, in which case, at least one these flags must
		 * match the node.
		 */
		std::atomic<unsigned long> _optionalFlags{0};

		/// List of id's of edges this action has already traversed.
		std::vector<unsigned> _traversedEdges;

		/**
		 * Traverse to the next node.
		 * @returns True if could traverse, false otherwise.
		 */
		bool __traverse();

		/**
		 * Execute a work unit for this action.
		 * @returns True if could execute the work unit, false otherwise.
		 */
		bool __executeWorkUnit();

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
