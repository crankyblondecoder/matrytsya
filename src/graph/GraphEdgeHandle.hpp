#ifndef GRAPH_EDGE_HANDLE_H
#define GRAPH_EDGE_HANDLE_H

class GraphEdge;

/**
 * Handle to a graph edge.
 * Guarantees that the graph edge this references will be available.
 * Graph edges will be automatically ref'd/de-ref'd.
 * @note This is not re-entrant.
 */
class GraphEdgeHandle
{
    public:

		/**
		 * Construct a handle from the given edge.
		 * @param edge Edge that handle refers to. May be null.
		 */
		GraphEdgeHandle(GraphEdge* edge);

		/**
		 * Create a new handle from another handle.
		 */
        GraphEdgeHandle(const GraphEdgeHandle& copyFrom);

		/**
		 * Edge handles can be re-assigned.
		 */
        GraphEdgeHandle& operator= (const GraphEdgeHandle& copyFrom);

		virtual ~GraphEdgeHandle();

		/**
		 * Get the graph edge contained in this handle.
		 * @returns Pointer to edge. Null if not available.
		 */
		GraphEdge* getEdge();

		/**
		 * Get whether the handle is valid. ie A valid edge pointer is available.
		 */
		bool isValid();

    protected:

    private:


		GraphEdge* _referencedEdge;
};

#endif
