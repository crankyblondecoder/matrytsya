#include "GraphHiveSceneSurfaceHtmlMap.hpp"

#include "../graph/GraphHiveSceneSurface.hpp"
#include "http/HttpRequest.hpp"

namespace
{
	/// How many poll intervals a page stays counted as a viewer without polling again. More than one, so that
	/// a poll held up a little, by a busy browser or a slow network, does not have it flicker out of the count.
	const unsigned _VIEWER_EXPIRY_POLLS = 3;

	/// Least time a page stays counted as a viewer without polling again, in milliseconds. Browsers slow the
	/// timers of a page in a background tab to around once a second, which would otherwise drop it out of the
	/// count between polls whenever the poll interval is short.
	const unsigned _MIN_VIEWER_EXPIRY_MS = 5000;
}

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

std::string GraphHiveSceneSurfaceHtmlMap::_serveRevisionExtras(HttpRequest& request)
{
	std::string viewerId = request.getQueryParam("viewerId");

	if(!viewerId.empty())
	{
		if(request.getQueryParam("leave") == "1")
		{
			_getSceneSurface().removeViewer(viewerId);
		}
		else
		{
			unsigned expiryMs = _VIEWER_EXPIRY_POLLS * _getPollInterval();

			if(expiryMs < _MIN_VIEWER_EXPIRY_MS) expiryMs = _MIN_VIEWER_EXPIRY_MS;

			_getSceneSurface().touchViewer(viewerId, expiryMs);
		}
	}

	return ",\"viewerCount\":" + std::to_string(_getSceneSurface().getViewerCount());
}
