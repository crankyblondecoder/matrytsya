#include "GraphHiveSceneSurfaceWebglMap.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>

#include "../graph/GraphHiveSceneSurface.hpp"
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
	GraphHiveSurfaceHttpMap(httpServer, surface, path), _sceneSurface{surface}
{
}

GraphHiveSceneSurfaceWebglMap::~GraphHiveSceneSurfaceWebglMap()
{
}

void GraphHiveSceneSurfaceWebglMap::_renderPage(HttpResponse& response)
{
	std::string page = webglPageTemplate;

	std::string title = _getSurface().getName();

	if(title.empty()) title = "Graph Hive Scene Surface";

	std::size_t titlePos = page.find("%TITLE%");

	while(titlePos != std::string::npos)
	{
		page.replace(titlePos, 7, title);

		titlePos = page.find("%TITLE%", titlePos + title.size());
	}

	response.setContentType("text/html");
	response.setBody(page);
}

void GraphHiveSceneSurfaceWebglMap::_serveData(HttpRequest& request, HttpResponse& response)
{
	(void) request;

	std::vector<GraphHiveSceneSurface::Chunk> chunks = _sceneSurface.getChunks();
	std::vector<GraphHiveSceneSurface::ModelTransform> modelTransforms = _sceneSurface.getModelTransforms();

	std::string json = "{\"modelTransforms\":[";

	for(std::size_t index = 0; index < modelTransforms.size(); index++)
	{
		if(index > 0) json += ",";

		json += "{\"id\":" + std::to_string(modelTransforms[index].id) +
			",\"transform\":" + jsonVec(modelTransforms[index].transform, 16) + "}";
	}

	json += "],\"chunks\":[";

	for(std::size_t chunkIndex = 0; chunkIndex < chunks.size(); chunkIndex++)
	{
		if(chunkIndex > 0) json += ",";

		const GraphHiveSceneSurface::Chunk& chunk = chunks[chunkIndex];

		json += "{\"modelTransformIndex\":" + std::to_string(chunk.modelTransformIndex) + ",\"vertexes\":[";

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
