#ifndef GRAPH_H
#define GRAPH_H

#include "GraphEdge.hpp"
#include "GraphNode.hpp"
#include "../thread/thread.hpp"
#include "../util/ptrList.hpp"

/**
 * Represents entire graph.
 * All sibling nodes (i.e. Those represented by edges from the same parent), SHOULD NOT (NEVER) affect
 * each others state. Only a parent node can affects a child state. This is VERY imporant because it
 * means this graph behaves quite differently from many existing "Scene Graphs". In summary the order of processing
 * sibling nodes is not set and should not be relied upon. You should therefore not try and set state on a child node
 * that affects other sibling nodes.
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
         * Add remove node from the graph.
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
