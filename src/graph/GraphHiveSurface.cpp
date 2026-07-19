#include "../util/EventListener.hpp"
#include "GraphException.hpp"
#include "GraphHiveSurface.hpp"
#include "GraphHiveSurfaceListener.hpp"

GraphHiveSurface::GraphHiveSurface(Type type) : _type(type), _hive(0)
{
}

GraphHiveSurface::Type GraphHiveSurface::getType()
{
	return _type;
}

bool GraphHiveSurface::getDefault()
{
	{ SYNC(_lock)

		return _default;
	}
}

void GraphHiveSurface::setDefault(bool isDefault)
{
	{ SYNC(_lock)

		_default = isDefault;
	}
}

void GraphHiveSurface::setHive(GraphHandle<GraphHive> hive)
{
	if(!hive.isValid())
	{
		throw GraphException(GraphException::INVALID_HIVE_HANDLE);
	}

	_hive = hive;
}

GraphHiveSurface::~GraphHiveSurface()
{
}

void GraphHiveSurface::poke(unsigned nodeId, GraphPoke poke)
{
	if(_hive.isValid())
	{
		_hive.getInstance() -> poke(nodeId, poke);
	}
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

bool GraphHiveSurface::populateStart()
{
	{ SYNC(_lock)

		if(_populating) return false;

		_populating = true;
	}

	_populateStart();

	return true;
}

void GraphHiveSurface::populateEnd()
{
	bool notify = false;

	{ SYNC(_lock)

		if(_populating)
		{
			_populating = false;
			notify = true;
		}
		else
		{
			throw GraphException(GraphException::HIVE_SURFACE_BAD_REQUEST);
		}
	}

	if(notify) _populateEnd();
}

bool GraphHiveSurface::isPopulating()
{
	{ SYNC(_lock)

		return _populating;
	}
}

