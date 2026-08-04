#ifndef GRAPH_HIVE_GRAPH_VIEW_SURFACE_H
#define GRAPH_HIVE_GRAPH_VIEW_SURFACE_H

#include <vector>

#include "../thread/ThreadMutex.hpp"
#include "GraphHive.hpp"
#include "GraphHiveSurface.hpp"
#include "GraphPoke.hpp"

/**
 * Surface that enables the graph structure of a hive, i.e. its nodes and the edges between them, to be
 * viewed and interacted with in a read only way.
 * @note The graph is taken from the hive itself rather than being walked by an action, so every node in the
 *       hive is reported whether or not it is reachable from any other.
 * @note Read only means that nothing offered here reaches back into the graph to change it. In particular a
 *       poke made against this surface is discarded rather than forwarded to the node it names.
 */
class GraphHiveGraphViewSurface : public GraphHiveSurface
{
	public:

		GraphHiveGraphViewSurface();

		/**
		 * The graph structure of a hive as of a single moment.
		 */
		struct Graph
		{
			/// Version of the hive the nodes were catalogued at. 0 before the first populate pass.
			unsigned version = 0;

			/// One entry per node the hive held, each naming the edges directed from it.
			std::vector<GraphHive::NodeCatalogueEntry> nodes;
		};

		virtual void activate() override;

		/**
		 * Get a copy of the graph this surface currently presents.
		 * @note A snapshot taken when called, so it neither changes underneath the caller nor keeps the
		 *       nodes it describes alive.
		 */
		Graph getGraph();

		/**
		 * Find a single node of the graph this surface currently presents by its id.
		 * @param nodeId Id of the node to find.
		 * @param node Filled in with a copy of the node when found, left untouched otherwise.
		 * @returns True if the graph holds a node with that id, false otherwise.
		 */
		bool getNode(unsigned nodeId, GraphHive::NodeCatalogueEntry& node);

		/**
		 * Discard a poke, as this surface is read only.
		 * @param nodeId The id of the node that would have been poked.
		 * @param poke Poke that is discarded.
		 */
		virtual void poke(unsigned nodeId, GraphPoke poke) override;

		virtual void strobe() override;

	protected:

		// Required by ref counting.
		virtual ~GraphHiveGraphViewSurface();

		virtual void _populateStart() override;

		virtual void _populateEnd() override;

		virtual void _close() override;

	private:

		// Disable copying.
		GraphHiveGraphViewSurface(const GraphHiveGraphViewSurface& copyFrom);
		GraphHiveGraphViewSurface& operator= (const GraphHiveGraphViewSurface& copyFrom);

		/**
		 * Catalogue the bound hive into a new graph, unless it is already presenting that version of it.
		 */
		void __populate();

		/// Nodes of the graph currently being built.
		std::vector<GraphHive::NodeCatalogueEntry> _nodes;

		/// The current graph that the surface can present. This is a kind of double buffering.
		Graph _currentGraph;

		/// Generic lock.
		ThreadMutex _lock;
};

#endif
