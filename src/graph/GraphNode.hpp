#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H

#include <atomic>

class GraphEdge;
class GraphHive;
class GraphNodeHandle;

#include "../util/RefCounted.hpp"
#include "GraphActionTargetable.hpp"
#include "GraphHiveHandle.hpp"
#include "GraphNamed.hpp"

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
         * @returns Handle to use to refer to the created edge (local to this node only). -1 if could not be added.
		 */
		int createEdge(GraphNodeHandle& connectTo);

		/**
		 * Remove edge from this node.
		 * @param handle Handle of edge to remove. As returned by createEdge.
		 */
        void removeEdge(int handle);

		/**
		 * Find the next node to traverse to.
		 * @returns Handle to next node to traverse to.
		 */
		GraphNodeHandle traverse();

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
		GraphHiveHandle getHive();

		/**
		 * Set the hive that this node belongs to.
		 * @param hive Hive that this node is part of.
		 * @returns True if this node accepts being part of the hive. False otherwise.
		 */
		bool setHive(GraphHiveHandle hive);

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

	private:

		/** The hive this node belongs to. */
		GraphHiveHandle _hive;

		/// All edges directed from this node.
        GraphEdge* _edges[EDGE_ARRAY_SIZE];

		/// Whether edge slot in _edges array has been allocated.
		bool _edgeAlloc[EDGE_ARRAY_SIZE];

		/// Count of the number of edges in the edge array.
		unsigned _edgeCount;

		/// Whether this node is in the process of decoupling or has been decoupled from all edges it contains.
		std::atomic<bool> _decoupled;

        /// Generic lock.
        ThreadMutex _lock;

		/** How much energy it costs for an action to be applied to this node. */
		unsigned _actionEnergyCost;

        // Do not allow copying.
        GraphNode(const GraphNode& copyFrom);
        GraphNode& operator= (const GraphNode& copyFrom);

		/**
		 * Remove edge from this node.
		 * @param handle Handle of edge to remove. As returned by createEdge.
		 * @returns Point to graph edge that needs to be deleted outside of lock.
		 */
        GraphEdge* __removeEdge(int handle);
};

#endif
