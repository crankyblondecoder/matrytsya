#include "GraphHiveSurfaceMap.hpp"

#include "../graph/GraphHiveSurface.hpp"

GraphHiveSurfaceMap::GraphHiveSurfaceMap(GraphHiveSurface& surface, std::string path) :
	_surface{surface}, _path{path}
{
}

GraphHiveSurfaceMap::~GraphHiveSurfaceMap()
{
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
