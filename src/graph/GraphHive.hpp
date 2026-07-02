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
		 * @note Expects to manage the initial reference count of this node regardless of whether it could be added.
		 */
		 void addNode(GraphNode* node);

		 /**
		  * Remove node from hive.
		  */
		 void removeNode(GraphNodeHandle nodeHandle);

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

		/**
		 * Wait on the initial (first) action becoming active within this hive.
		 * This will only wait on the very first action becoming active in this hive.
		 * @note This is intended to only be used for unit testing purposes.
		 * @param timeOut Maximum period in ms to wait on condition to be signalled. Use 0 for no timeout.
		 */
		void waitOnInitialActionActive(unsigned timeOut);

		/**
		 * Wait on the accumulated active action count going greater of equal to a particular value.
		 * @param count Value that count accumulator must be greater or equal to.
		 * @param timeOut Maximum period in ms to wait on condition to be signalled. Use 0 for no timeout.
		 */
		void waitOnActionActiveCountAccum(int count, unsigned timeOut);

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

		/**
		 * Number of currently active actions within the hive.
		 * A value of -1 indicates shutdown is in progress.
		 */
		int _activeActionCount = 0;

		/**
		 * The accumulated number of actions that have become active.
		 * A value of -1 indicates shutdown is in progress;
		 */
		int _activeActionCountAccum = 0;

		/// This flag indiciates that an initial action became active. This will only ever flip to true once.
		bool _initialActionActive = false;

		/// Condition that guards the current active action count.
		ThreadCondition _activeActionCountCond;

		/// Condition that guards the active action count accumulator.
		ThreadCondition _activeActionCountAccumCond;
};

#endif
