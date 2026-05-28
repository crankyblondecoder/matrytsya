#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H

class Graph;
class GraphEdge;
class GraphNodeHandle;

//#include "GraphAction.hpp"
#include "GraphActionTargetable.hpp"
#include "../util/RefCounted.hpp"

// The number of edges a node can have is fixed.
#define EDGE_ARRAY_SIZE 32

/**
 * Node of a graph
 * @note Nodes self delete when no longer referenced by an edge.
 */
class GraphNode : public RefCounted, public GraphActionTargetable
{
    public:

		/**
		 * Create new graph node.
		 * @note Because this is ref-counted it will require the automatic initial reference increase to be released
		 *       before it can be deleted. Doing this explicitly is only required if this node has _not_ had its
		 *       ref-count explicitly increased.
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
		 * @note It is imperitive that the ref count to this node is increased before calling this function.
		 * @param handle Handle of edge to remove. As returned by createEdge.
		 */
        void removeEdge(int handle);

		/**
		 * Inform this node that an edge is now pointing to it.
		 */
		void referredTo(GraphEdge* edge);

		/**
		 * Find the next node to traverse to.
		 * @returns Handle to next node to traverse to.
		 */
		GraphNodeHandle traverse();

		/**
		 * Get the energy cost of an action being applied to this node.
		 */
		unsigned getEnergyCost();

    protected:

		// Must be virtual for reference counting auto-delete.
		virtual ~GraphNode();

		/**
		 * Sub-class hook to do any required initialisation.
		 * @note The sub-class _should_ register any action flags it supports during this call.
		 */
		virtual void _init() = 0;

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

		/** All edges directed from this node. */
        GraphEdge* _edges[EDGE_ARRAY_SIZE];

		/** Count of the number of edges in the edge array. */
		unsigned _edgeCount;

        /** Generic lock. */
        ThreadMutex _lock;

		/** Whether the initial refcount has been removed. */
		bool _initRefcountRemoved;

		/** How much energy it costs for an action to be applied to this node. */
		unsigned _actionEnergyCost;

        // Do not allow copying.
        GraphNode(const GraphNode& copyFrom);
        GraphNode& operator= (const GraphNode& copyFrom);
};

#endif
