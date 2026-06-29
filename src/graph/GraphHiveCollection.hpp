#ifndef GRAPH_HIVE_COLLECTION_H
#define GRAPH_HIVE_COLLECTION_H

#include <map>
#include <string>

class GraphHiveHandle;

/**
 * Collection of hives.
 * Allows hives to be found by
 */
class GraphHiveCollection
{
	public:

		/**
		 * Add a hive to this collection.
		 * @note Hives names must be unique within a collection.
		 */
		void addHive(GraphHiveHandle hiveHandle);

		/**
		 * Shutdown all hives contained in this collection and remove them from the collection.
		 */
		void shutdown();

	protected:

	private:

		/// Map of graph hive name to the hives handle.
		std::map<std::string, GraphHiveHandle> _hives;
};

#endif
