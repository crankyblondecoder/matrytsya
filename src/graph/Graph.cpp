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

        // TODO ...
    }
}

void Graph::removeNode(GraphNode* node)
{
	node -> decouple();

    { SYNC(lock)

		// TODO ...
    }
}
