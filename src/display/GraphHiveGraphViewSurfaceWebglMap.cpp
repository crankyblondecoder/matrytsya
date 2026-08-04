#include "GraphHiveGraphViewSurfaceWebglMap.hpp"

#include <cstddef>
#include <stdio.h>

#include "../graph/graphActionFlagRegister.hpp"
#include "../graph/GraphHiveGraphViewSurface.hpp"
#include "../graph/GraphNodeType.hpp"
#include "graphViewPageTemplate.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace
{
	// Quotes a string as a JSON string literal, escaping what JSON does not allow to appear raw within one.
	// Needed here because a node's name is whatever the hive it came from called it, rather than the numbers
	// the rest of this payload is made of.
	std::string jsonString(const std::string& value)
	{
		std::string result = "\"";

		for(char character : value)
		{
			switch(character)
			{
				case '"':  result += "\\\""; break;
				case '\\': result += "\\\\"; break;
				case '\b': result += "\\b"; break;
				case '\f': result += "\\f"; break;
				case '\n': result += "\\n"; break;
				case '\r': result += "\\r"; break;
				case '\t': result += "\\t"; break;

				default:

					// Anything else below a space has no shorthand and may not appear raw, so goes out as an
					// escape. The cast keeps a high byte of a multi-byte UTF-8 sequence from looking negative
					// and being escaped as though it were a control character.
					if((unsigned char) character < 0x20)
					{
						char escape[7];

						snprintf(escape, sizeof(escape), "\\u%04x", (unsigned char) character);

						result += escape;
					}
					else
					{
						result += character;
					}

					break;
			}
		}

		return result + "\"";
	}

	// Names the concrete type of a node, so the page can show what a node is and colour it by its kind. The
	// names are the enum's own, which is what someone reading the hive's source would recognise it by.
	const char* nodeTypeName(GraphNodeType type)
	{
		switch(type)
		{
			case GraphNodeType::GRAPH_NODE: return "GRAPH_NODE";
			case GraphNodeType::PING_NODE: return "PING_NODE";
			case GraphNodeType::SCENE_GEOMETRY_NODE: return "SCENE_GEOMETRY_NODE";
			case GraphNodeType::SCENE_TRANSFORM_NODE: return "SCENE_TRANSFORM_NODE";
			case GraphNodeType::SCRIPT_NODE: return "SCRIPT_NODE";
			case GraphNodeType::SCENE_GEOMETRY_SCRIPT_NODE: return "SCENE_GEOMETRY_SCRIPT_NODE";
			case GraphNodeType::SCENE_TRANSFORM_SCRIPT_NODE: return "SCENE_TRANSFORM_SCRIPT_NODE";
			case GraphNodeType::SCENE_ROOT_NODE: return "SCENE_ROOT_NODE";
			case GraphNodeType::TELEPORT_NODE: return "TELEPORT_NODE";
			case GraphNodeType::AGENT_NODE: return "AGENT_NODE";
			case GraphNodeType::TRIGGER_NODE: return "TRIGGER_NODE";
		}

		return "GRAPH_NODE";
	}

	// Appends the names of the action flags an edge carries as a JSON array. The names match the ones a hive
	// file names the same flags by, so what the page shows of an edge can be read straight back against the
	// hive it was built from. An edge carrying no flags gives an empty array rather than being left out, so
	// the page can say so rather than showing nothing.
	void appendActionNames(std::string& json, unsigned long actionFlags)
	{
		struct ActionFlagName
		{
			unsigned long flag;
			const char* name;
		};

		static const ActionFlagName actionFlagNames[] =
		{
			{PING_GRAPH_ACTION, "PING_GRAPH_ACTION"},
			{SERIALISABLE_GRAPH_ACTION, "SERIALISABLE_GRAPH_ACTION"},
			{SCRIPT_GRAPH_ACTION, "SCRIPT_GRAPH_ACTION"},
			{SCENE_GRAPH_ACTION, "SCENE_GRAPH_ACTION"},
			{SCENE_STROBE_GRAPH_ACTION, "SCENE_STROBE_GRAPH_ACTION"},
			{ANIMATE_GRAPH_ACTION, "ANIMATE_GRAPH_ACTION"},
			{VERSION_GRAPH_ACTION, "VERSION_GRAPH_ACTION"},
			{AGENT_GRAPH_ACTION, "AGENT_GRAPH_ACTION"},
			{TRIGGER_GRAPH_ACTION, "TRIGGER_GRAPH_ACTION"},
			{AGENT_AFFECT_GRAPH_ACTION, "AGENT_AFFECT_GRAPH_ACTION"}
		};

		json += "[";

		bool first = true;

		for(const ActionFlagName& actionFlagName : actionFlagNames)
		{
			if(!(actionFlags & actionFlagName.flag)) continue;

			if(!first) json += ",";

			json += "\"";
			json += actionFlagName.name;
			json += "\"";

			first = false;
		}

		json += "]";
	}
}

GraphHiveGraphViewSurfaceWebglMap::GraphHiveGraphViewSurfaceWebglMap(HttpServerBase& httpServer,
	GraphHiveGraphViewSurface& surface, std::string path) :
	GraphHiveSurfaceHtmlMap(httpServer, surface, path), _graphViewSurface(&surface)
{
}

GraphHiveGraphViewSurfaceWebglMap::~GraphHiveGraphViewSurfaceWebglMap()
{
}

void GraphHiveGraphViewSurfaceWebglMap::_renderPage(HttpResponse& response)
{
	_renderPageTemplate(graphViewPageTemplate, response);
}

void GraphHiveGraphViewSurfaceWebglMap::_serveMapData(HttpRequest& request, HttpResponse& response)
{
	GraphHiveGraphViewSurface::Graph graph = _graphViewSurface.getInstance() -> getGraph();

	std::string json = "{\"version\":" + std::to_string(graph.version) + ",\"nodes\":[";

	for(std::size_t nodeIndex = 0; nodeIndex < graph.nodes.size(); nodeIndex++)
	{
		if(nodeIndex > 0) json += ",";

		const GraphHive::NodeCatalogueEntry& node = graph.nodes[nodeIndex];

		json += "{\"id\":" + std::to_string(node.id) +
			",\"name\":" + jsonString(node.name) +
			",\"type\":\"" + nodeTypeName(node.type) + "\",\"edges\":[";

		for(std::size_t edgeIndex = 0; edgeIndex < node.edges.size(); edgeIndex++)
		{
			if(edgeIndex > 0) json += ",";

			json += "{\"toNodeId\":" + std::to_string(node.edges[edgeIndex].toNodeId) + ",\"actions\":";

			appendActionNames(json, node.edges[edgeIndex].actionFlags);

			json += "}";
		}

		json += "]}";
	}

	response.setContentType("application/json");
	response.setBody(json + "]}");
}
