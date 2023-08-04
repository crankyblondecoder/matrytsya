#ifndef GRAPH_H
#define GRAPH_H

#include "GraphEdge.hpp"
#include "GraphNode.hpp"
#include "../thread/thread.hpp"

/**
 * Central place to handle common graph operations.
 * Manages node handle related functionality.
 */
class Graph
{
    public:

        virtual ~Graph();
        Graph(GraphNode* rootNode);

		// TODO Some kind of paged array that indexes of are also the nodes' handles ...
		blah;

		// TODO Some kind of dictionary that can associate nodes with identifiers ...
		// Dictionary keys are the handle and the values are also just unsigned ...
		blah;

        /**
         * Add node to the graph.
         * @param node Node to add. Graph owns node.
         */
        void addNode(GraphNode*);

		/**
         * Remove node from the graph.
         * @param node Node to remove.
         */
        void removeNode(GraphNode*);

    private:

        GraphNode* rootNode;
        ThreadMutex lock;
};

#endif
