#ifndef GRAPH_H
#define GRAPH_H

#include "GraphEdge.hpp"
#include "GraphNode.hpp"
#include "../thread/thread.hpp"
#include "../util/ptrList.hpp"

/**
 * Represents entire graph.
 */
class Graph
{
    public:

        virtual ~Graph();
        Graph(GraphNode* rootNode);

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

        /**
         * Apply action to the graph.
         * @param action Action to apply to graph.
         */
        void applyAction(GraphAction*);

    private:

        GraphNode* rootNode;
        ptrList<GraphNode> nodes;
        ThreadMutex lock;
};

#endif
