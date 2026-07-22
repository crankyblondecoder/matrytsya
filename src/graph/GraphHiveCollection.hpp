#ifndef GRAPH_HIVE_COLLECTION_H
#define GRAPH_HIVE_COLLECTION_H

#include <string>
#include <vector>

#include "./actions/SerialisableActionPayload.hpp"
#include "GraphNodeLocation.hpp"

template <typename T> class Handle;
class GraphHive;

/**
 * Collection of hives.
 * Allows hives to communicate with each other.
 */
class GraphHiveCollection
{
	public:

		/**
		 * Add a hive to this collection.
		 * @note Hives names must be unique within a collection.
		 */
		void addHive(Handle<GraphHive> hiveHandle);

		/**
		 * Get the hive with the given name.
		 * @returns A graph handle of the hive if found. Invalid handle otherwise.
		 */
		Handle<GraphHive> getHive(std::string hiveName);

		/**
		 * Teleport a graph action.
		 * @param actionPayload Payload of action to teleport.
		 * @param nodeLocation Location of node to teleport action to.
		 */
		void teleportAction(SerialisableActionPayload& actionPayload, GraphNodeLocation& nodeLocation);

		/**
		 * Shutdown all hives contained in this collection and remove them from the collection.
		 * @note This should never be called by anything other then the owner of this collection.
		 */
		void shutdown();

	protected:

	private:

		/// Map of graph hive name to the hives handle.
		std::vector<Handle<GraphHive>*> _hives;

        /// Generic lock.
        ThreadMutex _lock;
};

#endif
