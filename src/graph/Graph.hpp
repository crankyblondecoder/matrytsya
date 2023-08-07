#ifndef GRAPH_H
#define GRAPH_H

#include "GraphEdge.hpp"
#include "GraphNode.hpp"
#include "../thread/thread.hpp"
#include "../util/ptrList.hpp"

#define NODE_LIST_PAGE_SIZE 65535

/**
 * Central place to handle common graph operations.
 * Manages node handle related functionality.
 */
class Graph
{
	friend GraphNode;

    public:

        virtual ~Graph();
        Graph();

    private:

        ThreadMutex _lock;

		/// Number of nodes this graph has.
		unsigned _numNodes;

		/// Node list pages. The pages should all be the same size.
		ptrList<NodeListPage> _nodeListPages;

		/**
         * Add node to the graph.
         * @param node Node to add.
		 * @returns Node handle.
		 * @throws GraphException.
         */
        unsigned __addNode(GraphNode* node);

		/**
         * Remove node from the graph.
         * @param handle Handle of node to remove.
		 * @throws GraphException.
         */
        void __removeNode(unsigned handle);

		/**
		 * Get a pointer to a node given its handle.
		 */
		GraphNode* __getNode(unsigned handle);
};

class NodeListPage
{
	public:

		~NodeListPage();

		NodeListPage(unsigned pageSize, unsigned handleOffset);

		/**
		 * Determine if this page can add an entry.
		 */
		bool canAddEntry();

		/**
		 * Add an entry to the page.
		 * @returns Handle for entry.
		 * @throws GraphException.
		 */
		unsigned addEntry(GraphNode* node);

		/**
		 * Remove an entry from the page.
		 * @throws GraphException.
		 */
		void removeEntry(unsigned handle);

		/**
		 * Get an entry from the page.
		 * @throws GraphException.
		 */
		GraphNode* getEntry(unsigned handle);

	private:

		/// Array of nodes this page contains.
		GraphNode** _nodeList;

		/// Total number of entries this page has.
		unsigned _pageSize;

		/// Number of entries currently used in the page.
		unsigned _entriesUsed;

		/// The next entry to search for an available entry from.
		unsigned _nextEntry;

		/// This number is added to the array index to get the node handle.
		unsigned _handleOffset;
};

#endif
