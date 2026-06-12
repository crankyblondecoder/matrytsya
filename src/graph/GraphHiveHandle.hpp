#ifndef GRAPH_HIVE_HANDLE_H
#define GRAPH_HIVE_HANDLE_H

class GraphHive;

/**
 * Handle to a graph node.
 * Guarantees that the graph node this references will be available.
 * Graph nodes will be automatically ref'd/de-ref'd.
 * @note This is not re-entrant.
 */
class GraphHiveHandle
{
    public:

		/**
		 * Construct a handle from the given hive.
		 * @param hive Hive that handle refers to. May be null.
		 */
		GraphHiveHandle(GraphHive* hive);

		/**
		 * Create a new handle from another handle.
		 */
        GraphHiveHandle(const GraphHiveHandle& copyFrom);

		/**
		 * Hive handles can be re-assigned.
		 */
        GraphHiveHandle& operator= (const GraphHiveHandle& copyFrom);

		virtual ~GraphHiveHandle();

		/**
		 * Get the graph node contained in this handle.
		 * @returns Pointer to node. Null if not available.
		 */
		GraphHive* getHive();

		/** Get whether the handle is valid. ie A valid hive pointer is available. */
		bool isValid();

    protected:

    private:

		GraphHive* _referencedHive;
};

#endif
