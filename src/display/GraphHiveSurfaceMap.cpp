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

GraphHiveSurface& GraphHiveSurfaceMap::_getSurface()
{
	return _surface;
}
