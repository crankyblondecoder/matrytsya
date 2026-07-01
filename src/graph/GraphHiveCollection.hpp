#ifndef GRAPH_HIVE_COLLECTION_H
#define GRAPH_HIVE_COLLECTION_H

#include <string>
#include <vector>

#include "./actions/SerialisableActionPayload.hpp"
#include "GraphNodeLocation.hpp"

class GraphHiveHandle;

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
		void addHive(GraphHiveHandle hiveHandle);

		/**
		 * Get the hive with the given name.
		 * @returns A graph handle of the hive if found. Invalid handle otherwise.
		 */
		GraphHiveHandle getHive(std::string hiveName);

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
		std::vector<GraphHiveHandle*> _hives;

        /// Generic lock.
        ThreadMutex _lock;
};

#endif
