#include "GraphHiveSceneSurfaceWebglMap.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdlib.h>

#include "../graph/GraphHiveSceneSurface.hpp"
#include "../graph/GraphPoke.hpp"
#include "../thread/thread.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "webglPageTemplate.hpp"

namespace
{
	std::string jsonNumber(double value)
	{
		std::ostringstream stream;

		stream << std::setprecision(9) << value;

		return stream.str();
	}

	std::string jsonVec(const double* values, unsigned count)
	{
		std::string result = "[";

		for(unsigned index = 0; index < count; index++)
		{
			if(index > 0) result += ",";

			result += jsonNumber(values[index]);
		}

		result += "]";

		return result;
	}

	std::string jsonByteVec(const std::byte* values, unsigned count)
	{
		std::string result = "[";

		for(unsigned index = 0; index < count; index++)
		{
			if(index > 0) result += ",";

			result += std::to_string(std::to_integer<unsigned>(values[index]));
		}

		result += "]";

		return result;
	}

	// Name that the viewer's JavaScript matches on to decide when a chunk is drawn; must stay in step with
	// the VertexVisibility.* handling in webglPageTemplate.cpp.
	const char* visibilityName(SceneGeometry::VertexVisibility visibility)
	{
		switch(visibility)
		{
			case SceneGeometry::VertexVisibility::ALWAYS:       return "ALWAYS";
			case SceneGeometry::VertexVisibility::GRABBED:      return "GRABBED";
			case SceneGeometry::VertexVisibility::DRAGGING:     return "DRAGGING";
			case SceneGeometry::VertexVisibility::HOVERED_OVER: return "HOVERED_OVER";
		}

		return "ALWAYS";
	}
}

GraphHiveSceneSurfaceWebglMap::GraphHiveSceneSurfaceWebglMap(HttpServerBase& httpServer, GraphHiveSceneSurface& surface,
	std::string path) :
	GraphHiveSurfaceHttpMap(httpServer, surface, path), _sceneSurface(&surface)
{
	surface.addListener(this);
}

GraphHiveSceneSurfaceWebglMap::~GraphHiveSceneSurfaceWebglMap()
{
}

void GraphHiveSceneSurfaceWebglMap::setPollInterval(unsigned pollIntervalMs)
{
	{ SYNC(_lock)

		_pollIntervalMs = pollIntervalMs;
	}
}

void GraphHiveSceneSurfaceWebglMap::_renderPage(HttpResponse& response)
{
	std::string page = webglPageTemplate;

	std::string title = _sceneSurface.getInstance() -> getName();
	unsigned pollIntervalMs;

	{ SYNC(_lock)

		pollIntervalMs = _pollIntervalMs;
	}

	if(title.empty()) title = "Graph Hive Scene Surface";

	std::size_t titlePos = page.find("%TITLE%");

	while(titlePos != std::string::npos)
	{
		page.replace(titlePos, 7, title);

		titlePos = page.find("%TITLE%", titlePos + title.size());
	}

	std::string pollIntervalStr = std::to_string(pollIntervalMs);
	std::size_t pollPos = page.find("%POLL_INTERVAL_MS%");

	while(pollPos != std::string::npos)
	{
		page.replace(pollPos, 18, pollIntervalStr);

		pollPos = page.find("%POLL_INTERVAL_MS%", pollPos + pollIntervalStr.size());
	}

	response.setContentType("text/html");
	response.setBody(page);
}

void GraphHiveSceneSurfaceWebglMap::__serveRevision(HttpResponse& response)
{
	unsigned revision;

	{ SYNC(_lock)

		revision = _revision;
	}

	response.setContentType("application/json");
	response.setBody("{\"revision\":" + std::to_string(revision) + "}");
}

void GraphHiveSceneSurfaceWebglMap::__servePoke(HttpRequest& request, HttpResponse& response)
{
	std::string nodeIdParam = request.getQueryParam("nodeId");
	std::string chunkIdParam = request.getQueryParam("chunkId");

	// Both are required: the node id routes the poke to the owning node, the chunk id identifies which of
	// that node's chunks was poked. Neither is unique enough to stand alone.
	if(nodeIdParam.empty() || chunkIdParam.empty())
	{
		response.setStatus(400);
		response.setContentType("application/json");
		response.setBody("{\"error\":\"missing nodeId or chunkId\"}");

		return;
	}

	unsigned nodeId = (unsigned) strtoul(nodeIdParam.c_str(), 0, 10);
	unsigned chunkId = (unsigned) strtoul(chunkIdParam.c_str(), 0, 10);

	GraphPoke::PokeType type = GraphPoke::PokeType::HIT;

	std::string typeParam = request.getQueryParam("type");

	if(typeParam == "hoverEnter") type = GraphPoke::PokeType::HOVER_ENTER;
	else if(typeParam == "hoverLeave") type = GraphPoke::PokeType::HOVER_LEAVE;

	GraphPoke::PokeData data{};

	_sceneSurface.getInstance() -> poke(nodeId, GraphPoke(type, data, chunkId));

	response.setContentType("application/json");
	response.setBody("{\"ok\":true}");
}

void GraphHiveSceneSurfaceWebglMap::hiveSurfaceChanged(GraphHandle<GraphHiveSurface> hiveSurface)
{
	{ SYNC(_lock)

		_revision++;
	}
}

GraphHiveSurfaceListener* GraphHiveSceneSurfaceWebglMap::getListener()
{
	return this;
}

void GraphHiveSceneSurfaceWebglMap::_serveData(HttpRequest& request, HttpResponse& response)
{
	std::string base = getPath();

	while(!base.empty() && base.back() == '/') base.pop_back();

	if(request.getPath() == base + "/revision")
	{
		__serveRevision(response);

		return;
	}

	if(request.getPath() == base + "/poke")
	{
		__servePoke(request, response);

		return;
	}

	GraphHiveSceneSurface* sceneSurface = _sceneSurface.getInstance();

	GraphHiveSceneSurface::Scene scene = sceneSurface -> getScene();

	std::string focusChunkIdsJson = "[";

	if(scene.hasInitialFocusNode)
	{
		bool first = true;

		for(const GraphHiveSceneSurface::Chunk& chunk : scene.chunks)
		{
			if(chunk.nodeId != scene.initialFocusNodeId) continue;

			if(!first) focusChunkIdsJson += ",";

			focusChunkIdsJson += std::to_string(chunk.id);

			first = false;
		}
	}

	focusChunkIdsJson += "]";

	std::string json = "{\"focusChunkIds\":" + focusChunkIdsJson +
		",\"focusViewportFraction\":" + jsonNumber(scene.focusViewportFraction) +
		",\"modelTransforms\":[";

	for(std::size_t index = 0; index < scene.modelTransforms.size(); index++)
	{
		if(index > 0) json += ",";

		json += "{\"id\":" + std::to_string(scene.modelTransforms[index].id) +
			",\"transform\":" + jsonVec(scene.modelTransforms[index].transform, 16) + "}";
	}

	json += "],\"chunks\":[";

	for(std::size_t chunkIndex = 0; chunkIndex < scene.chunks.size(); chunkIndex++)
	{
		if(chunkIndex > 0) json += ",";

		const GraphHiveSceneSurface::Chunk& chunk = scene.chunks[chunkIndex];

		json += "{\"id\":" + std::to_string(chunk.id) +
			",\"nodeId\":" + std::to_string(chunk.nodeId) +
			",\"modelTransformIndex\":" + std::to_string(chunk.modelTransformIndex) +
			",\"pokeable\":" + (chunk.pokeable ? "true" : "false") +
			",\"visibility\":\"" + visibilityName(chunk.visibility) + "\",\"vertexes\":[";

		for(std::size_t vertexIndex = 0; vertexIndex < chunk.vertexes.size(); vertexIndex++)
		{
			if(vertexIndex > 0) json += ",";

			const Vertex& vertex = chunk.vertexes[vertexIndex];

			json += "{\"posn\":" + jsonVec(vertex.posn, 3) +
				",\"colour\":" + jsonByteVec(vertex.colour, 4) +
				",\"texCoords\":" + jsonVec(vertex.texCoords, 2) +
				",\"normal\":" + jsonVec(vertex.normal, 3) + "}";
		}

		json += "]}";
	}

	json += "]}";

	response.setContentType("application/json");
	response.setBody(json);
}
