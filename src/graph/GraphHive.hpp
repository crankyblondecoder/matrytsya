#ifndef GRAPH_HIVE_H
#define GRAPH_HIVE_H

#include <atomic>
#include <vector>

#include "GraphNodeHandle.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../thread/ThreadPool.hpp"

class GraphNode;

/**
 * Types of nodes a have can create and contain.
 */
enum GraphHiveNodeType
{
	/** Simple ping node. */
	PING = 1,
	/** Capable of routing to different edges depending on action. */
	ROUTE = 2,
	/** Can teleport an action between hives. */
	TELEPORT = 3
};

/**
 * A "Hive" is a container for nodes.
 * Nodes can refer to the hive when they want access to specific services, like persistence for example.
 */
class GraphHive
{
    public:

        virtual ~GraphHive();

		/**
		 * @param numThreads The number of threads to create for the hive to do processing on.
		 */
        GraphHive(unsigned numThreads);

		/**
		 * Add a graph node to this hive.
		 * @note Expects to manage the initial reference count of this node.
		 * @returns Nodes hive index. -1 for couldn't add node.
		 */
		 unsigned addNode(GraphNode* node);

		 /**
		  * Remove node from hive.
		  */
		 void removeNode(unsigned nodeIndex);

	protected:

    private:

		/// Thread pool that hive runs actions on.
		ThreadPool* _threadPool;

		/// Nodes contained in this hive.
		std::vector<GraphNode*>	_nodes;

		/// Whether this hive is active.
		std::atomic<bool> _active;

		/// Thread condition that guards whether this hive is active.
		ThreadCondition _activeCond;

        /// Generic lock.
        ThreadMutex _lock;


};

#endif
