#include "GraphHiveSceneSurfaceWebglMap.hpp"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include "../graph/GraphHiveSceneSurface.hpp"
#include "../rapidjson/document.h"
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

	// A chunk id is only unique within its owning node, so both are combined into a single key to identify a
	// chunk the viewer already holds a copy of.
	uint64_t chunkKey(unsigned nodeId, unsigned chunkId)
	{
		return (static_cast<uint64_t>(nodeId) << 32) | chunkId;
	}

	// Parses the viewer's list of chunks it already holds vertex data for, sent as the data request's body:
	// {"chunks":[{"nodeId":N,"chunkId":N,"vertexVersion":N}, ...]}. An empty or malformed body is treated the
	// same as an empty list, i.e. the viewer is assumed to know nothing yet and gets every chunk's vertexes.
	std::unordered_map<uint64_t, unsigned> parseKnownChunkVersions(const std::string& body)
	{
		std::unordered_map<uint64_t, unsigned> known;

		if(body.empty()) return known;

		rapidjson::Document document;

		document.Parse(body.c_str());

		if(document.HasParseError() || !document.IsObject() || !document.HasMember("chunks") ||
			!document["chunks"].IsArray())
		{
			return known;
		}

		for(auto& chunkValue : document["chunks"].GetArray())
		{
			if(!chunkValue.IsObject() || !chunkValue.HasMember("nodeId") || !chunkValue.HasMember("chunkId") ||
				!chunkValue.HasMember("vertexVersion"))
			{
				continue;
			}

			if(!chunkValue["nodeId"].IsUint() || !chunkValue["chunkId"].IsUint() ||
				!chunkValue["vertexVersion"].IsUint())
			{
				continue;
			}

			known[chunkKey(chunkValue["nodeId"].GetUint(), chunkValue["chunkId"].GetUint())] =
				chunkValue["vertexVersion"].GetUint();
		}

		return known;
	}
}

GraphHiveSceneSurfaceWebglMap::GraphHiveSceneSurfaceWebglMap(HttpServerBase& httpServer, GraphHiveSceneSurface& surface,
	std::string path) :
	GraphHiveSceneSurfaceHtmlMap(httpServer, surface, path)
{
}

GraphHiveSceneSurfaceWebglMap::~GraphHiveSceneSurfaceWebglMap()
{
}

void GraphHiveSceneSurfaceWebglMap::_renderPage(HttpResponse& response)
{
	_renderPageTemplate(webglPageTemplate, response);
}

void GraphHiveSceneSurfaceWebglMap::_serveMapData(HttpRequest& request, HttpResponse& response)
{
	std::unordered_map<uint64_t, unsigned> knownChunkVersions = parseKnownChunkVersions(request.getBody());

	GraphHiveSceneSurface::Scene scene = _getSceneSurface().getScene();

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
			",\"version\":" + std::to_string(chunk.version) +
			",\"vertexVersion\":" + std::to_string(chunk.vertexVersion) +
			",\"modelTransformIndex\":" + std::to_string(chunk.modelTransformIndex) +
			",\"pokeable\":" + (chunk.pokeable ? "true" : "false") +
			",\"visibility\":\"" + visibilityName(chunk.visibility) + "\",\"vertexes\":";

		auto knownIt = knownChunkVersions.find(chunkKey(chunk.nodeId, chunk.id));

		if(knownIt != knownChunkVersions.end() && knownIt -> second == chunk.vertexVersion)
		{
			// The viewer already has this chunk's vertexes cached at the current version, so there is no need
			// to resend them.
			json += "null";
		}
		else
		{
			json += "[";

			for(std::size_t vertexIndex = 0; vertexIndex < chunk.vertexes.size(); vertexIndex++)
			{
				if(vertexIndex > 0) json += ",";

				const Vertex& vertex = chunk.vertexes[vertexIndex];

				json += "{\"posn\":" + jsonVec(vertex.posn, 3) +
					",\"colour\":" + jsonByteVec(vertex.colour, 4) +
					",\"texCoords\":" + jsonVec(vertex.texCoords, 2) +
					",\"normal\":" + jsonVec(vertex.normal, 3) + "}";
			}

			json += "]";
		}

		json += "}";
	}

	json += "]}";

	response.setContentType("application/json");
	response.setBody(json);
}
