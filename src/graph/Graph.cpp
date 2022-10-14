#include "Graph.hpp"

Graph::~Graph()
{
    nodes.clear();
}

Graph::Graph(GraphNode* rootNode)
    : rootNode(rootNode)
{
    addNode(rootNode);
}

void Graph::addNode(GraphNode* node)
{
    { SYNC(lock)

        nodes.append(node, true);
    }
}

void Graph::applyAction(GraphAction* action)
{
    rootNode -> applyAction(action);
}
