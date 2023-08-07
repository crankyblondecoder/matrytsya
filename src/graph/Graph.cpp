#include "Graph.hpp"
#include "GraphException.hpp"

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
			try
			{
				curPage = new NodeListPage(NODE_LIST_PAGE_SIZE, _nodeListPages.count() * NODE_LIST_PAGE_SIZE);
			}
			catch(const std::bad_alloc& ex)
			{
				throw GraphException(GraphException::NODE_LIST_PAGE_BAD_ALLOC);
			}

			// Ptrlist doesn't do out of memory exception handling.
			try
			{
				_nodeListPages.append(curPage, true);
			}
			catch(const std::bad_alloc& ex)
			{
				throw GraphException(GraphException::NODE_LIST_PAGE_BAD_ALLOC);
			}
		}
		else
		{
			// The last page added is most likely to contain unallocated entries.
			curPage = _nodeListPages.last();

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
			// Should not have ever made it to here with a null current page.
			throw GraphException(GraphException::NODE_LIST_PAGE_UNEXPECTED);
		}

		try
		{
			curPage -> addEntry(node);
		}
		catch(const GraphException& ex)
		{
			// Should never get to here if code is correct.
			throw GraphException(GraphException::NODE_LIST_PAGE_UNEXPECTED);
		}

		_numNodes++;
    }
}

void Graph::__removeNode(unsigned handle)
{
    { SYNC(_lock)

		// Assume pages are a fixed size of NODE_LIST_PAGE_SIZE.
		NodeListPage* curPage = _nodeListPages.atIndex(handle / NODE_LIST_PAGE_SIZE);

		if(curPage)
		{
			curPage -> removeEntry(handle);
		}
		else
		{
			throw GraphException(GraphException::INVALID_NODE_HANDLE);
		}
    }
}

GraphNode* Graph::__getNode(unsigned handle)
{
	GraphNode* retNode = 0;

	{ SYNC(_lock)

		// Assume pages are a fixed size of NODE_LIST_PAGE_SIZE.
		NodeListPage* curPage = _nodeListPages.atIndex(handle / NODE_LIST_PAGE_SIZE);

		if(curPage)
		{
			try
			{
				retNode = curPage -> getEntry(handle);
			}
			catch(const GraphException& ex)
			{
				throw GraphException(GraphException::NODE_NOT_FOUND);
			}
		}
		else
		{
			throw GraphException(GraphException::INVALID_NODE_HANDLE);
		}
    }

	return retNode;
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
	else
	{
		throw GraphException(GraphException::NODE_LIST_PAGE_FULL);
	}

	return _handleOffset + _nextEntry;
}

void NodeListPage::removeEntry(unsigned handle)
{
	unsigned entryIndex = handle - _handleOffset;

	if(entryIndex < _pageSize)
	{
		_nodeList[entryIndex] = 0;
		_entriesUsed--;
	}
	else
	{
		throw GraphException(GraphException::NODE_LIST_PAGE_ITEM_NOT_FOUND);
	}
}

GraphNode* NodeListPage::getEntry(unsigned handle)
{
	unsigned entryIndex = handle - _handleOffset;

	if(entryIndex < _pageSize)
	{
		return _nodeList[entryIndex];
	}
	else
	{
		throw GraphException(GraphException::NODE_LIST_PAGE_ITEM_NOT_FOUND);
	}
}
