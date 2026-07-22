#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H

#include <atomic>
#include <queue>
#include <vector>

class GraphAction;
class GraphEdge;
class GraphHive;

#include "../util/RefCounted.hpp"
#include "GraphActionTargetable.hpp"
#include "../util/Handle.hpp"
#include "GraphNamed.hpp"
#include "GraphPoke.hpp"

// The number of edges a node can have is fixed.
#define EDGE_ARRAY_SIZE 32

/**
 * Node of a graph
 * @note Nodes self delete when no longer referenced by an edge.
 */
class GraphNode : public RefCounted, public GraphActionTargetable, public GraphNamed
{
    public:

		/**
		 * Create new graph node.
		 * @note Because this is ref-counted it will require the automatic initial reference increase to be released
		 *       before it can be deleted.
		 */
        GraphNode();

		/**
		 * Create and add an edge from this node to another node.
		 * ie The edge is directed from this node to another node.
		 * @note Only nodes can create edges.
		 * @param connectTo Handle of node to form edge to.
		 * @param actionFlags Action flags to add to edge. These will determine which actions can traverse this edge.
		 *        Leave blank for no restriction.
         * @returns Handle of created edge. Will be invalid if edge could not be created.
		 */
		Handle<GraphEdge> createEdge(Handle<GraphNode>& connectTo, std::vector<unsigned long> actionFlags);

		/**
		 * Remove edge from this node.
		 * @param handle Handle of edge to remove.
		 */
        void removeEdge(Handle<GraphEdge> handle);

		/**
		 * Find the next node to traverse to.
		 * @note This defines default traversal behaviour which is to traverse the next edge that the action has not
		 *       already traversed.
		 * @param action Action that is requesting to traverse.
		 * @returns Handle to next edge to traverse.
		 */
		virtual Handle<GraphEdge> traverse(GraphAction& action);

		/**
		 * Get the energy cost of an action being applied to this node.
		 */
		unsigned getEnergyCost();

		/**
		 * Does any house keeping associated with decoupling from the graph.
		 * Once a node is decoupled, it can't be re-attached.
		 * @note This can be assumed to be associated with being removed from a hive.
		 */
		void decouple();

		/**
		 * Get a handle to the hive this node is part of.
		 */
		Handle<GraphHive> getHive();

		/**
		 * Get the unique id of this node.
		 */
		unsigned getId();

		/**
		 * Set the hive that this node belongs to.
		 * @param hive Hive that this node is part of.
		 * @returns True if this node accepts being part of the hive. False otherwise.
		 */
		bool setHive(Handle<GraphHive> hive);

		/**
		 * Poke this node.
		 */
		virtual void poke(GraphPoke poke) final;

		/**
		 * Get whether poking is enabled.
		 */
		bool getPokeEnabled();

		/**
		 * Set whether poking is enabled.
		 */
		void setPokeEnabled(bool enable);

		/**
		 * Schedule a graph action to be applied on this node.
		 * If the action can't be processed immediately, it is placed on a queue for later execution.
		 * @note The order of action application to this node is preserved.
		 * @param action Action to schedule to apply to this node.
		 * @returns True if could be scheduled. False otherwise.
		 */
		bool scheduleAction(Handle<GraphAction> action);

		/**
		 * Call in point for a thread work unit to process a scheduled action and potentially create a new
		 * work unit for further processing if the action queue is not empty.
		 * @param abort If true, the thread pool work unit was not assigned a thread.
		 */
		void processScheduledAction(bool abort);

    protected:

		// Must be virtual for reference counting auto-delete.
		virtual ~GraphNode();

		/**
		 * Emit an action by making its origin this node.
		 * @note All subclasses must use this function to emit actions so that correct binding to the node occurs.
		 * @param action Action to emit. This must have its refcount increased prior to the call.
		 */
		void _emitAction(GraphAction* action);

		/**
		 * Set the energy cost of an action being applied to this node.
		 */
		void _setEnergyCost(unsigned cost);

		/**
		 * Subclass hook to notify that this node has been poked.
		 */
		virtual void _poked(GraphPoke poke) = 0;

	private:

        /// Generic lock.
        ThreadMutex _lock;

		/// Counter used to derive each node's unique id.
		static std::atomic<unsigned> _nextId;

		/// Unique id of this node.
		unsigned _id;

		/// The hive this node belongs to.
		Handle<GraphHive> _hive;

		/// All edges directed from this node.
        GraphEdge* _edges[EDGE_ARRAY_SIZE]{};

		/// Whether edge slot in _edges array has been allocated.
		bool _edgeAlloc[EDGE_ARRAY_SIZE]{};

		/// Count of the number of edges in the edge array.
		unsigned _edgeCount = 0;

		/// Whether this node is in the process of decoupling or has been decoupled from all edges it contains.
		std::atomic<bool> _decoupled{false};

		/// How much energy it costs for an action to be applied to this node.
		unsigned _actionEnergyCost = 1;

		/// Whether poking is enabled for this node. If false, all pokes are immediately discarded.
		bool _pokeEnabled = false;

		/// Queue of actions waiting to be applied to this node.
		std::queue<Handle<GraphAction>> _scheduledActions;

		/// Whether a work unit is currently active, or about to become active, for draining _scheduledActions.
		bool _scheduledActionProcessing = false;

        // Do not allow copying.
        GraphNode(const GraphNode& copyFrom);
        GraphNode& operator= (const GraphNode& copyFrom);

		/**
		 * Remove edge from this node.
		 * @note This method needs to be externally locked.
		 * @param edgeIndex Index into edges array of edge to remove.
		 * @returns Point to graph edge that needs to be deleted outside of lock.
		 */
        GraphEdge* __removeEdge(int edgeIndex);

		/**
		 * Create and submit a work unit to the hive to continue draining the scheduled action queue.
		 * @note Must be called with _scheduledActionProcessing already set to true and outside of _lock.
		 * @returns True if the work unit could be submitted. False otherwise.
		 */
		bool __executeScheduledActionWorkUnit();
};

#endif
