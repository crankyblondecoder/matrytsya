#ifndef HIVE_RECORDER_H
#define HIVE_RECORDER_H

#include <map>
#include <string>
#include <vector>

#include "HiveNodeDescriptor.hpp"
#include "HiveSnapshot.hpp"
#include "HiveSurfaceDescriptor.hpp"
#include "../graph/GraphHiveListener.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../util/EventEmitter.hpp"
#include "../util/EventListener.hpp"
#include "../util/Handle.hpp"
#include "../util/RefCounted.hpp"

class GraphHive;
class HiveLoader;

/**
 * Keeps a record of what a running hive would have to be rebuilt from.
 *
 * What is recorded for each node is what it held as it joined the hive: what it was loaded with, or for a node
 * added while the hive runs, what it held when added. A node replaced by another of the same name is recorded as
 * the replacement held it when it joined. Its edges, whether a builder has used its one build up, and the hive's
 * strobe registrations are taken as they stand whenever a snapshot is made, as these are what a hive's growth
 * changes. Nothing a script changes about a node after it joins - a transform being animated, say - is recorded.
 *
 * @note stop() must be called before this is released if start() was called: until then the hive holds a
 *       reference to this as one of its listeners.
 */
class HiveRecorder : public RefCounted, private EventListener<GraphHiveListener>
{
	public:

		/**
		 * @param hive Hive to record. Must already hold every node the loader describes.
		 * @param loader Loader the hive was built from, which gives each of its nodes as it was loaded and the
		 *        order its nodes and surfaces go in.
		 */
		HiveRecorder(Handle<GraphHive> hive, HiveLoader& loader);

		/**
		 * Start listening for nodes joining and leaving the hive, so that each is recorded as it joins.
		 * @note Safe to call more than once.
		 */
		void start();

		/**
		 * Stop listening to the hive.
		 * @note Safe to call more than once, and without start() having been called.
		 */
		void stop();

		/**
		 * Get the name of the recorded hive.
		 */
		std::string getHiveName();

		/**
		 * Take a snapshot of the hive as it now stands.
		 * @returns The snapshot. Holds no nodes once the hive has been shut down.
		 * @throw PersistException(HIVE_NOT_CAPTURABLE) If a node joined that cannot be described.
		 */
		HiveSnapshot snapshot();

		/**
		 * Get the record of one node, with its edges as they now stand.
		 * @param nodeName Name of the node.
		 * @param descriptor Set to the node's record if it is in the hive.
		 * @returns True if the node is in the hive.
		 * @throw PersistException(HIVE_NOT_CAPTURABLE) If the node cannot be described.
		 */
		bool getNode(const std::string& nodeName, HiveNodeDescriptor& descriptor);

	protected:

		// Required by ref counting.
		virtual ~HiveRecorder();

	private:

		// Disable copying.
		HiveRecorder(const HiveRecorder& copyFrom);
		HiveRecorder& operator= (const HiveRecorder& copyFrom);

		virtual void hiveNodeAdded(EventEmitter<GraphHiveListener>& emitter) override;

		virtual void hiveNodeRemoved(EventEmitter<GraphHiveListener>& emitter) override;

		virtual void populateEventListenerHandle(CastHandle<GraphHiveListener>& handle) override;

		/**
		 * Record every node in the hive that is not yet recorded, or that has been replaced since it was.
		 */
		void __captureNewNodes();

		/**
		 * Forget every recorded node that is no longer in the hive.
		 */
		void __forgetRemovedNodes();

		/**
		 * Get a node's record with its edges and build state as they now stand.
		 * @param nodeName Name of the node.
		 * @param descriptor Set to the node's record if it is recorded and still in the hive.
		 * @returns True if it is.
		 */
		bool __liveNode(const std::string& nodeName, HiveNodeDescriptor& descriptor);

		/// Hive recorded.
		Handle<GraphHive> _hive;

		/// Name of the hive.
		std::string _hiveName;

		/// Names of the hive's nodes, in the order they are to be rebuilt in. Guarded by _lock.
		std::vector<std::string> _nodeOrder;

		/// Each node as it joined the hive, by name. Edges and build state are left as captured, and replaced
		/// whenever read. Guarded by _lock.
		std::map<std::string, HiveNodeDescriptor> _nodes;

		/// Id of the node each record was taken from, by name, so that a node replaced under the same name is
		/// noticed. Guarded by _lock.
		std::map<std::string, unsigned> _nodeIds;

		/// The hive's surfaces, in the order they are to be rebuilt in. These never change while a hive runs.
		std::vector<HiveSurfaceDescriptor> _surfaces;

		/// Whether this is listening to the hive. Guarded by _lock.
		bool _listening = false;

		/// Generic lock.
		ThreadMutex _lock;
};

#endif
