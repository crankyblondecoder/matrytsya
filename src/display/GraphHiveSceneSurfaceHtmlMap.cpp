#include "GraphHiveSceneSurfaceHtmlMap.hpp"

#include "../graph/GraphHiveSceneSurface.hpp"
#include "http/HttpRequest.hpp"

#include <iomanip>
#include <sstream>

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

	unsigned focusSequence;
	unsigned focusNodeId;
	double focusViewportFraction;

	_getSceneSurface().getFocusRequest(focusSequence, focusNodeId, focusViewportFraction);

	std::ostringstream fractionStream;

	fractionStream << std::setprecision(9) << focusViewportFraction;

	// Carried by the revision rather than the scene data, so that a page is told of a focus request on its very
	// next poll without having to fetch a scene that has not changed.
	return ",\"viewerCount\":" + std::to_string(_getSceneSurface().getViewerCount()) +
		",\"focus\":{\"sequence\":" + std::to_string(focusSequence) +
		",\"nodeId\":" + std::to_string(focusNodeId) +
		",\"viewportFraction\":" + fractionStream.str() + "}";
}
