#include "../util/EventListener.hpp"
#include "GraphHiveSurface.hpp"
#include "GraphHiveSurfaceListener.hpp"

GraphHiveSurface::GraphHiveSurface()
{
}

GraphHiveSurface::~GraphHiveSurface()
{
}

void GraphHiveSurface::_emitSurfaceChanged()
{
	std::list<EventListener<GraphHiveSurfaceListener>*> listeners = _getListeners();

	for(EventListener<GraphHiveSurfaceListener>* listener : listeners)
	{
		// The listener knows how to get the concrete listener interface.
		GraphHiveSurfaceListener* surfListener = listener -> getListener();

		if(surfListener) surfListener -> hiveSurfaceChanged(GraphHandle<GraphHiveSurface>(this));
	}
}

void GraphHiveSurface::close()
{
	_close();

	decrRef();
}

