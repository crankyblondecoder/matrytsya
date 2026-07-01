#ifndef GRAPH_HIVE_H
#define GRAPH_HIVE_H

#include <vector>

#include "GraphNamed.hpp"
#include "GraphNodeHandle.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../thread/ThreadPool.hpp"
#include "../util/RefCounted.hpp"

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

	protected:

		// Required by ref counting.
        virtual ~GraphHive();

    private:

		/// Thread pool that hive runs actions on.
		ThreadPool* _threadPool = nullptr;

		/// Nodes contained in this hive.
		std::vector<GraphNode*>	_nodes;

		/// Whether this hive is active.
		bool _active = false;

        /// Generic lock.
        ThreadMutex _lock;
};

#endif
