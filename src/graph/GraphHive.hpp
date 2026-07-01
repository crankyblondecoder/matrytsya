#ifndef GRAPH_HIVE_H
#define GRAPH_HIVE_H

#include <vector>

#include "../thread/ThreadCondition.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../thread/ThreadPool.hpp"
#include "../util/RefCounted.hpp"
#include "./actions/SerialisableActionPayload.hpp"
#include "GraphHiveCollection.hpp"
#include "GraphNamed.hpp"
#include "GraphNodeLocation.hpp"
#include "GraphNodeHandle.hpp"

class GraphNode;

/**
 * A "Hive" is a container for nodes.
 * Nodes can refer to the hive when they want access to specific services, like persistence for example.
 */
class GraphHive : public RefCounted, public GraphNamed
{
    public:

		/**
		 * Constructor
		 * @note Will wait on its internal thread pool to become active.
		 * @param numThreads The number of threads to create for the hive to do processing on.
		 */
		GraphHive(unsigned numThreads);

		/**
		 * Shutdown this hive.
		 * This will dismantle the hive's graph.
		 */
		void shutdown();

		/**
		 * Add a graph node to this hive.
		 * @note Expects to manage the initial reference count of this node.
		 * @returns Nodes hive index. -1 for couldn't add node.
		 */
		 int addNode(GraphNode* node);

		 /**
		  * Remove node from hive.
		  */
		 void removeNode(unsigned nodeIndex);

		 /**
		  * Find a node in this hive by name.
		  * @param nodeName Name of node to find.
		  * @returns Handle to the node. Invalid handle if no node with that name exists in this hive.
		  */
		 GraphNodeHandle getNode(std::string nodeName);

		 /**
		  * Get the thread pool used by this hive to enumerate itself.
		  * @param numTabs Number of tabs to indent output by.
		  */
		 void enumerateThreadPool(unsigned numTabs);

		/**
		 * Execute a work unit using this hives thread pool.
		 * @note The work unit will be automatically deleted once it has completed.
		 * @param workUnit Work unit to execute.
		 * @returns True if could execute. False otherwise.
		 */
		bool executeWorkUnit(ThreadPoolWorkUnit* workUnit);

		/**
		 * Set the graph hive collection this hive is part of.
		 */
		void setHiveCollection(GraphHiveCollection* collection);

		/**
		 * Teleport a graph action.
		 * @param actionPayload Payload of action to teleport.
		 * @param nodeLocation Location of node to teleport action to.
		 */
		void teleportAction(SerialisableActionPayload& actionPayload, GraphNodeLocation& nodeLocation);

		/**
		 * Notify this hive that a graph action has become active.
		 * This occurs when the action starts.
		 * @returns Handle to use when notifying this hive that the action has become inactive.
		 */
		unsigned actionActive(GraphAction* action);

		/**
		 * Notify this have that a graph action has become inactive.
		 * This occurs when the action is complete.
		 * @param handle Handle that was supplied by actionActive.
		 */
		void actionInactive(unsigned handle);

		/**
		 * Wait on there being no active actions within this hive.
		 * @param timeOut Maximum period in ms to wait on condition to be signalled. Use 0 for no timeout.
		 */
		void waitOnNoActionsActive(unsigned timeOut);

	protected:

		// Required by ref counting.
        virtual ~GraphHive();

    private:

		/// Thread pool that hive runs actions on.
		ThreadPool* _threadPool = 0;

		/// Hive collection this hive is part of.
		GraphHiveCollection* _collection = 0;

		/// Nodes contained in this hive.
		std::vector<GraphNode*>	_nodes;

		/// Currently active actions within the hive.
		std::vector<GraphAction*> _activeActions;

		/// Whether this hive is active.
		bool _active = false;

        /// Generic lock.
        ThreadMutex _lock;

		/// Number of currently active actions within the hive. Guarded solely by _noActionsActiveCond's own mutex.
		unsigned _activeActionCount = 0;

		/// Condition signalled whenever the active action count drops to zero.
		ThreadCondition _noActionsActiveCond;
};

#endif
