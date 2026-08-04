#include "GraphHiveGraphViewSurface.hpp"

#include <utility>

#include "../thread/thread.hpp"

namespace
{
	// Determine if two catalogued edges describe the same edge.
	bool edgesEqual(const GraphHive::EdgeCatalogueEntry& a, const GraphHive::EdgeCatalogueEntry& b)
	{
		return a.toNodeId == b.toNodeId && a.actionFlags == b.actionFlags;
	}

	// Determine if two catalogues describe the same graph. Order is taken to be significant, as a hive
	// catalogues its nodes, and each node its edges, in a stable order.
	bool cataloguesEqual(const std::vector<GraphHive::NodeCatalogueEntry>& a,
		const std::vector<GraphHive::NodeCatalogueEntry>& b)
	{
		if(a.size() != b.size()) return false;

		for(unsigned index = 0; index < a.size(); index++)
		{
			const GraphHive::NodeCatalogueEntry& nodeA = a[index];
			const GraphHive::NodeCatalogueEntry& nodeB = b[index];

			if(nodeA.id != nodeB.id || nodeA.type != nodeB.type || nodeA.name != nodeB.name) return false;

			if(nodeA.edges.size() != nodeB.edges.size()) return false;

			for(unsigned edgeIndex = 0; edgeIndex < nodeA.edges.size(); edgeIndex++)
			{
				if(!edgesEqual(nodeA.edges[edgeIndex], nodeB.edges[edgeIndex])) return false;
			}
		}

		return true;
	}
}

GraphHiveGraphViewSurface::GraphHiveGraphViewSurface() : GraphHiveSurface(Type::GRAPH_VIEW_SURFACE)
{
}

GraphHiveGraphViewSurface::~GraphHiveGraphViewSurface()
{
}

void GraphHiveGraphViewSurface::activate()
{
	__populate();
}

void GraphHiveGraphViewSurface::strobe()
{
	__populate();
}

void GraphHiveGraphViewSurface::poke(unsigned nodeId, GraphPoke poke)
{
	// Deliberately not forwarded to the hive. This surface presents the graph, it does not drive it.
}

void GraphHiveGraphViewSurface::__populate()
{
	Handle<GraphHive> hive = _getHive();

	if(!hive.isValid()) return;

	GraphHive* hiveInstance = hive.getInstance();

	// Read before the catalogue is taken, so that a change made while it is being taken leaves this surface
	// stamped with the older version and the next populate pass picks that change up rather than missing it.
	unsigned version = hiveInstance -> getVersion();

	if(getPopulateVersion() == version) return;

	std::vector<GraphHive::NodeCatalogueEntry> catalogue = hiveInstance -> catalogueNodes();

	if(!populateStart(version)) return;

	{ SYNC(_lock)

		_nodes = std::move(catalogue);
	}

	populateEnd();
}

void GraphHiveGraphViewSurface::_populateStart()
{
	{ SYNC(_lock)

		_nodes.clear();
	}
}

void GraphHiveGraphViewSurface::_populateEnd()
{
	// Read outside the lock, as the base class takes a lock of its own to answer it.
	unsigned version = getPopulateVersion();

	bool graphChanged = false;

	{ SYNC(_lock)

		graphChanged = !cataloguesEqual(_nodes, _currentGraph.nodes);

		if(graphChanged) _currentGraph.nodes = std::move(_nodes);

		// Stamped whether or not anything changed, so that the surface records the version of the hive it is
		// known to agree with rather than the last one that happened to differ.
		_currentGraph.version = version;

		_nodes.clear();
	}

	if(graphChanged) _emitSurfaceChanged();
}

GraphHiveGraphViewSurface::Graph GraphHiveGraphViewSurface::getGraph()
{
	{ SYNC(_lock)

		return _currentGraph;
	}
}

bool GraphHiveGraphViewSurface::getNode(unsigned nodeId, GraphHive::NodeCatalogueEntry& node)
{
	{ SYNC(_lock)

		for(const GraphHive::NodeCatalogueEntry& found : _currentGraph.nodes)
		{
			if(found.id != nodeId) continue;

			node = found;

			return true;
		}
	}

	return false;
}

void GraphHiveGraphViewSurface::_close()
{
}
