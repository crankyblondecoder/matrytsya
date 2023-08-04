#include "Graph.hpp"

Graph::~Graph()
{
}

Graph::Graph()
{
	_numNodeListPages = 1;
	_numNodeListAllocCurPage = 0;

	_nodeListPages.append(new GraphNode*[NODE_LIST_PAGE_SIZE], true);
}

unsigned Graph::__addNode(GraphNode* node)
{
    { SYNC(_lock)

        // TODO ...
    }
}

void Graph::__removeNode(unsigned handle)
{
    { SYNC(_lock)

		// TODO ...
    }
}
