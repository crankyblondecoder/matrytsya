#include "Graph.hpp"

Graph::~Graph()
{
	_nodeListPages.clear();
}

Graph::Graph()
{
	_nodeListPages.append(new NodeListPage(NODE_LIST_PAGE_SIZE, 0), true);
}

unsigned Graph::__addNode(GraphNode* node)
{
    { SYNC(_lock)

		unsigned maxNumNodes = _nodeListPages.count() * NODE_LIST_PAGE_SIZE;

		NodeListPage* curPage = 0;

		if(_numNodes >= maxNumNodes)
		{
			// TODO check for memory allocation error and throw exception if a problem ...

			curPage = new NodeListPage(NODE_LIST_PAGE_SIZE, _nodeListPages.count() * NODE_LIST_PAGE_SIZE);

			_nodeListPages.append(curPage, true);
		}
		else
		{
			curPage = _nodeListPages.current();

			if(!curPage -> canAddEntry())
			{
				curPage = _nodeListPages.first();

				while(curPage && !curPage -> canAddEntry())
				{
					curPage = _nodeListPages.next();
				}
			}
		}

		if(!curPage)
		{
			// TODO should not have ever made it to here with a null current page ...
		}

		curPage -> addEntry(node);

		_numNodes++;
    }
}

void Graph::__removeNode(unsigned handle)
{
    { SYNC(_lock)

		// TODO ...
    }
}

// *** NodeListPage ***

NodeListPage::~NodeListPage()
{
	delete[] _nodeList;
}

NodeListPage::NodeListPage(unsigned pageSize, unsigned handleOffset)
{
	_handleOffset = handleOffset;
	_pageSize = pageSize;
	_entriesUsed = 0;
	_nextEntry = 0;

	_nodeList = new GraphNode*[pageSize];

	for(unsigned index = 0; index < pageSize; index++)
	{
		_nodeList[index] = 0;
	}
}

bool NodeListPage::canAddEntry()
{
	return _entriesUsed < _pageSize;
}

unsigned NodeListPage::addEntry(GraphNode* node)
{
	if(_entriesUsed < _pageSize)
	{
		while(_nodeList[_nextEntry])
		{
			_nextEntry++;

			if(_nextEntry >= _pageSize) _nextEntry = 0;
		}

		_nodeList[_nextEntry] = node;

		_entriesUsed++;
	}

	return _handleOffset + _nextEntry;
}

void NodeListPage::removeEntry(unsigned handle)
{
	unsigned entryIndex = handle - _handleOffset;

	_nodeList[entryIndex] = 0;

	_entriesUsed--;
}
