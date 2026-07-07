#include "GraphHiveSurfaceHttpMap.hpp"

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpServerBase.hpp"

GraphHiveSurfaceHttpMap::GraphHiveSurfaceHttpMap(HttpServerBase& httpServer, GraphHiveSurface& surface,
	std::string path) :
	GraphHiveSurfaceMap(surface, path), _httpServer{httpServer}
{
	_httpServer.addHandler(getPath(), this);
}

GraphHiveSurfaceHttpMap::~GraphHiveSurfaceHttpMap()
{
	_httpServer.removeHandler(getPath());
}

void GraphHiveSurfaceHttpMap::handleRequest(HttpRequest& request, HttpResponse& response)
{
	std::string requestPath = request.getPath();
	std::string path = getPath();

	if(requestPath == path || requestPath == path + "/")
	{
		_renderPage(response);
	}
	else
	{
		_serveData(request, response);
	}
}
