#ifndef GRAPH_NODE_HANDLE_H
#define GRAPH_NODE_HANDLE_H

class GraphNode;

/**
 * Handle to a graph node.
 * Guarantees that the graph node this references will be available while an instance of this class exists.
 * Graph nodes will be automatically ref'd/de-ref'd.
 * @note This is not re-entrant.
 */
class GraphNodeHandle
{
    public:

		/**
		 * Construct a handle from the given node.
		 * @param node Node that handle refers to. May be null.
		 */
		GraphNodeHandle(GraphNode* node);

		/**
		 * Create a new handle from another handle.
		 */
        GraphNodeHandle(const GraphNodeHandle& copyFrom);

		/**
		 * Node handles can be re-assigned.
		 */
        GraphNodeHandle& operator= (const GraphNodeHandle& copyFrom);

		virtual ~GraphNodeHandle();

		/**
		 * Get the graph node contained in this handle.
		 * @returns Pointer to node. Null if not available.
		 */
		GraphNode* getNode();

		/**
		 * Get whether the handle is valid. ie A valid node pointer is available.
		 */
		bool isValid();

		/**
		 * Clear the handle, i.e. De-reference the pointed to node and make this handle invalid.
		 */
		void clear();

    protected:

    private:

		GraphNode* _referencedNode;
};

#endif
