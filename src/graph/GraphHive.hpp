#ifndef GRAPH_HIVE_H
#define GRAPH_HIVE_H

#include "GraphNodeHandle.hpp"
#include "../thread/ThreadMutex.hpp"

#include <vector>

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
        GraphHive();

		/**
		 * Add a graph node to this hive.
		 * @note Expects to manage the initial reference count of this node.
		 * @returns Nodes hive index.
		 */
		 unsigned addNode(GraphNode* node);

		 /**
		  * Remove node from hive.
		  */
		 void removeNode(unsigned nodeIndex);

	protected:

    private:

		/** Nodes contained in this hive. */
		std::vector<GraphNode*> _nodes;

        /** Generic lock. */
        ThreadMutex _lock;

};

#endif
