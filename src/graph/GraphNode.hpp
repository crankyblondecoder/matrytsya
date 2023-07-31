#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H

class GraphEdge;

#include "GraphAction.hpp"
#include "../util/RefCounted.hpp"
#include "../thread/thread.hpp"

// The number of edges a node can have is fixed.
#define EDGE_ARRAY_SIZE 32

/** Node of a graph */
class GraphNode : private RefCounted
{
	friend GraphEdge;

    public:

		/**
		 * Create new graph node.
		 * @note Because this is refcounted it will require the automatic initial refcount to be released before it can
		 * be deleted.
		 */
        GraphNode();

		/**
		 * Get the maximum number edges that can be attached to this node.
		 */
		int getMaxNumAttachedEdges();

		/**
		 * Form and edge from this node to another node.
		 * ie The edge is directed from this node to another node.
		 * @note At this stage only nodes can create edges.
		 * @param node Node to form edge to.
		 * @param traversalFlags Flags that control what can traverse the edge.
		 * @returns True if edge could be formed. False otherwise.
		 */
		bool formEdgeTo(GraphNode* node, unsigned long traversalFlags);

		/**
		 * Decouple from the graph.
		 * This essentially detaches all edges and stops the node from having any new edges attached to it.
		 */
		void decouple();

		/**
		 * Traverse edges attached to this node and return the next node to apply an action.
		 * @note Make sure this is called on the actions own thread.
		 * @note Once an action calls this it should _not_ keep any pointer to this node.
		 * @param action Action that wants to traverse edge. It is assumed this action currently holds a reference to the node.
		 * @returns A ref counted node pointer or null if could not traverse.
		 */
		GraphNode* traverse(GraphAction* action);

		/**
		 * Must be called by an action when it is bound to this node but won't traverse it.
		 */
		void wontTraverse();

    protected:

		// Must be virtual for reference counting auto-delete.
		virtual ~GraphNode();

		/**
		 * Emit an action by making its origin this node.
		 * @note All subclasses must use this function to emit actions so that correct binding to the node occurs.
		 * @param action Action to emit.
		 */
		void _emitAction(GraphAction* action);

		/**
		 * Determine if an action can target this node.
		 * ie This node has the target interface required of the action.
		 */
		virtual bool _canActionTarget(GraphAction*) = 0;

    private:

        /// All edges directed either from or to this node.
        GraphEdge* _edges[EDGE_ARRAY_SIZE];

		/// Number of edges currently present.
		unsigned _edgeCount;

		/// Helper for allocating edges without having to search for spare spots.
		/// This count allows edges to be initially allocated right up to the end of the edge array before having to start
		/// searching the array for empty slots.
		unsigned _linearEdgeAllocCount;

        // Generic lock.
        ThreadMutex _lock;

		// Whether this node is decoupling from the graph.
		bool _decoupling;

        // Do not allow copying.
        GraphNode(const GraphNode& copyFrom);
        GraphNode& operator= (const GraphNode& copyFrom);

		/**
		 * Helper function to form an edge between two nodes.
		 * @param fromNode Node to form edge from.
		 * @param toNode Node to form edge to.
		 * @param traversalFlags Flags that determine if action can traverse an edge.
		 * @returns True if edge could be formed. False otherwise.
		 */
		bool __formEdge(GraphNode* fromNode, GraphNode* toNode, unsigned long traversalFlags);

		/**
         * Add edge which is directed either from or to this node.
         * @param edge Edge to add.
         * @returns Handle to use to refer to edge with respect to this node. -1 if could not be added.
         */
        int __addEdge(GraphEdge* edge);

		/**
		 * Remove edge from this node.
		 * Assume this is called as part of edge detachment.
		 * @param handle Handle of edge to remove.
		 */
        void __removeEdge(int handle);

		/**
		 * Get the edge a particular action should traverse.
		 * @param action Action to propogate.
		 * @returns Edge to traverse. This will have had its refcount incremented and _must_ be decremented when pointer is no
		 *          longer required.
		 * @note This should only be called by an action on its own thread.
		 */
		GraphEdge* __findEdgeToTraverse(GraphAction* action);
};

#endif
