#include "GraphHiveSceneSurfaceHtmlMap.hpp"

#include "../graph/GraphHiveSceneSurface.hpp"

GraphHiveSceneSurfaceHtmlMap::GraphHiveSceneSurfaceHtmlMap(HttpServerBase& httpServer, GraphHiveSceneSurface& surface,
	std::string path) :
	GraphHiveSurfaceHtmlMap(httpServer, surface, path), _sceneSurface(&surface)
{
}

GraphHiveSceneSurfaceHtmlMap::~GraphHiveSceneSurfaceHtmlMap()
{
}

GraphHiveSceneSurface& GraphHiveSceneSurfaceHtmlMap::_getSceneSurface()
{
	return *_sceneSurface.getInstance();
}
