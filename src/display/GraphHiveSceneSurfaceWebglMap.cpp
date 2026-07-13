#include "GraphHiveSceneSurfaceWebglMap.hpp"

#include <array>
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
	std::string chunkIdParam = request.getQueryParam("chunkId");

	if(chunkIdParam.empty())
	{
		response.setStatus(400);
		response.setContentType("application/json");
		response.setBody("{\"error\":\"missing chunkId\"}");

		return;
	}

	unsigned chunkId = (unsigned) strtoul(chunkIdParam.c_str(), 0, 10);

	_sceneSurface.getInstance() -> poke(chunkId, GraphPoke(GraphPoke::PokeType::HIT, std::array<int, 4>{0, 0, 0, 0}));

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

	std::string json = "{\"modelTransforms\":[";

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
			",\"modelTransformIndex\":" + std::to_string(chunk.modelTransformIndex) + ",\"vertexes\":[";

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
