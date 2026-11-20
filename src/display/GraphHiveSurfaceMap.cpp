#include "GraphHiveSurfaceMap.hpp"

#include "../graph/GraphHiveSurface.hpp"

GraphHiveSurfaceMap::GraphHiveSurfaceMap(GraphHiveSurface& surface, std::string path) :
	_surface{surface}, _path{path}
{
	// Held for as long as this map is, as a hive can now be removed while a request is still being answered
	// from one of its surfaces' maps.
	_surface.incrRef();
}

GraphHiveSurfaceMap::~GraphHiveSurfaceMap()
{
	_surface.decrRef();
}

std::string GraphHiveSurfaceMap::getPath()
{
	return _path;
}

std::string GraphHiveSurfaceMap::getSurfaceName()
{
	return _surface.getName();
}

GraphHiveSurface& GraphHiveSurfaceMap::_getSurface()
{
	return _surface;
}
