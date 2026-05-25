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
 * @note Nodes self delete when no longer referenced. They keep a self reference until either decoupled or all edges have
 *       been removed that refer to the node.
 */
class GraphNode : public RefCounted, public GraphActionTargetable
{
    public:

		/**
		 * Create new graph node.
		 * @note Because this is refcounted it will require the automatic initial refcount to be released before it can
		 * 		 be deleted. Doing this explicitly is only required if no edges were attached to this node and can be done by
		 *       calling decouple().
		 */
        GraphNode();

		/**
		 * Create and add an edge from this node to another node.
		 * ie The edge is directed from this node to another node.
		 * @note Only nodes can create edges.
		 * @param connectTo Handle of node to form edge to.
		 * @param traversalFlags Flags that control what can traverse the edge.
         * @returns Handle to use to refer to the created edge (local to this node only). -1 if could not be added.
		 */
		int createEdge(GraphNodeHandle& connectTo, unsigned long traversalFlags);

		/**
		 * Remove edge from this node.
		 * @param handle Handle of edge to remove. As returned by createEdge.
		 */
        void removeEdge(int handle);

		/**
		 * Find next node to apply given action to.
		 * @note Make sure this is only called on the actions own thread.
		 * @note Once an action calls this it should _not_ keep any pointer to _this_ node.
		 * @param action Action that wants to traverse this node.
		 * @returns A ref incr node pointer or null if could not traverse.
		 */
		GraphNode* traverse(GraphAction* action);

		/**
		 * Get the energy cost of an action being applied to this node.
		 */
		unsigned getActionEnergyCost();

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
		 * @param action Action to emit.
		 */
		void _emitAction(GraphAction* action);

		/**
		 * Set the energy cost of an action being applied to this node.
		 */
		void _setActionEnergyCost(unsigned cost);

	private:

		/** All edges directed from this node. */
        GraphEdge* _edges[EDGE_ARRAY_SIZE];

        /** Generic lock. */
        ThreadMutex _lock;

		/** How much energy it costs for an action to be applied to this node. */
		unsigned _actionEnergyCost;

        // Do not allow copying.
        GraphNode(const GraphNode& copyFrom);
        GraphNode& operator= (const GraphNode& copyFrom);
};

#endif
