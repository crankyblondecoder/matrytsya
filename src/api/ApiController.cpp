#include "ApiController.hpp"

#include "ApiException.hpp"
#include "HiveHost.hpp"
#include "apiDocs.hpp"
#include "../agent/ModelToolCallParameterValue.hpp"
#include "../agent_bindings/TriggerEmitterToolBindings.hpp"
#include "../display/http/HttpRequest.hpp"
#include "../display/http/HttpResponse.hpp"
#include "../graph/GraphEdge.hpp"
#include "../graph/GraphHive.hpp"
#include "../graph/GraphHiveCollection.hpp"
#include "../graph/GraphHiveSurface.hpp"
#include "../graph/GraphNode.hpp"
#include "../graph/GraphPoke.hpp"
#include "../graph/actionTargets/AnimateActionTarget.hpp"
#include "../graph/nodes/StrobeEmitterNode.hpp"
#include "../persist/HiveBuilder.hpp"
#include "../persist/HiveNodeDescriptor.hpp"
#include "../persist/HiveRecorder.hpp"
#include "../persist/HiveSnapshot.hpp"
#include "../persist/PersistException.hpp"
#include "../persist/hiveNames.hpp"
#include "../persist/json/JsonHiveLoader.hpp"
#include "../persist/json/JsonHiveWriter.hpp"
#include "../persist/lmdb/LmdbHiveStore.hpp"
#include "../rapidjson/document.h"
#include "../rapidjson/error/en.h"
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/writer.h"

#include <algorithm>
#include <ctype.h>
#include <stdlib.h>

namespace
{
	typedef rapidjson::Writer<rapidjson::StringBuffer> JsonWriter;

	/// Version of the API, which is also the last segment of its base path.
	const char* const _API_VERSION = "1";

	/// Base path of the API, which every path the API hands out starts with.
	const char* const _BASE_PATH = "/api/v1";

	void writeString(JsonWriter& writer, const std::string& value)
	{
		writer.String(value.c_str(), (rapidjson::SizeType) value.size());
	}

	void writeKeyString(JsonWriter& writer, const char* key, const std::string& value)
	{
		writer.Key(key);
		writeString(writer, value);
	}

	std::string finish(rapidjson::StringBuffer& buffer)
	{
		return std::string(buffer.GetString(), buffer.GetSize());
	}

	void respondJson(HttpResponse& response, unsigned status, const std::string& body)
	{
		response.setStatus(status);
		response.setContentType("application/json");
		response.setBody(body);
	}

	// Decodes a URL encoded path segment. Unlike a query value, a '+' in a path is a plus sign, not a space.
	std::string decodeSegment(const std::string& value)
	{
		std::string decoded;

		for(std::size_t index = 0; index < value.size(); index++)
		{
			char character = value[index];

			if(character == '%' && index + 2 < value.size() && isxdigit((unsigned char) value[index + 1]) &&
				isxdigit((unsigned char) value[index + 2]))
			{
				decoded += (char) strtol(value.substr(index + 1, 2).c_str(), 0, 16);

				index += 2;
			}
			else
			{
				decoded += character;
			}
		}

		return decoded;
	}

	std::vector<std::string> splitPath(const std::string& path)
	{
		std::vector<std::string> segments;

		std::size_t start = 0;

		while(start < path.size())
		{
			if(path[start] == '/')
			{
				start++;
				continue;
			}

			std::size_t end = path.find('/', start);

			if(end == std::string::npos) end = path.size();

			segments.push_back(path.substr(start, end - start));

			start = end;
		}

		return segments;
	}

	// Matches a request's raw path segments against a route's pattern, collecting its parameters if it matches.
	bool matchPattern(const std::string& pattern, const std::vector<std::string>& segments,
		std::map<std::string, std::string>& params)
	{
		std::vector<std::string> patternSegments = splitPath(pattern);

		if(patternSegments.size() != segments.size()) return false;

		std::map<std::string, std::string> matched;

		for(std::size_t index = 0; index < segments.size(); index++)
		{
			const std::string& patternSegment = patternSegments[index];

			if(patternSegment.size() > 2 && patternSegment.front() == '{' && patternSegment.back() == '}')
			{
				std::string decoded = decodeSegment(segments[index]);

				if(decoded.empty()) return false;

				matched[patternSegment.substr(1, patternSegment.size() - 2)] = decoded;
			}
			else if(patternSegment != segments[index])
			{
				return false;
			}
		}

		params = matched;

		return true;
	}

	// Encodes a name for use as one segment of a path handed back to a client.
	std::string encodeSegment(const std::string& value)
	{
		static const char* const HEX = "0123456789ABCDEF";

		std::string encoded;

		for(unsigned char character : value)
		{
			if(isalnum(character) || character == '-' || character == '_' || character == '.' || character == '~')
			{
				encoded += (char) character;
			}
			else
			{
				encoded += '%';
				encoded += HEX[character >> 4];
				encoded += HEX[character & 0x0f];
			}
		}

		return encoded;
	}

	// Parses a request body that has to be a JSON object. An empty body is taken as an empty object when allowed.
	void parseBody(HttpRequest& request, rapidjson::Document& document, bool emptyAllowed)
	{
		std::string body = request.getBody();

		if(body.find_first_not_of(" \t\r\n") == std::string::npos)
		{
			if(!emptyAllowed) throw ApiException(ApiException::BAD_REQUEST, "A JSON object body is required.");

			document.SetObject();
			return;
		}

		document.Parse(body.c_str(), body.size());

		if(document.HasParseError())
		{
			throw ApiException(ApiException::BAD_REQUEST, std::string("The body is not valid JSON: ") +
				rapidjson::GetParseError_En(document.GetParseError()) + " (at offset " +
				std::to_string(document.GetErrorOffset()) + ").");
		}

		if(!document.IsObject()) throw ApiException(ApiException::BAD_REQUEST, "The body must be a JSON object.");
	}

	std::string toJson(const rapidjson::Value& value)
	{
		rapidjson::StringBuffer buffer;
		JsonWriter writer(buffer);

		value.Accept(writer);

		return finish(buffer);
	}

	// Gets an optional string member, refusing one of the wrong type.
	std::string optionalString(rapidjson::Document& document, const char* name)
	{
		if(!document.HasMember(name)) return "";

		if(!document[name].IsString())
		{
			throw ApiException(ApiException::UNPROCESSABLE, std::string("\"") + name + "\" must be a string.");
		}

		return std::string(document[name].GetString(), document[name].GetStringLength());
	}

	unsigned requiredPeriod(rapidjson::Document& document)
	{
		if(!document.HasMember("periodMs") || !document["periodMs"].IsUint() || document["periodMs"].GetUint() == 0)
		{
			throw ApiException(ApiException::UNPROCESSABLE, "\"periodMs\" must be a whole number of milliseconds "
				"greater than zero.");
		}

		return document["periodMs"].GetUint();
	}

	// Writes every edge leaving a node, each with the id it can be removed by.
	void writeEdges(JsonWriter& writer, GraphNode& node)
	{
		writer.Key("edges");
		writer.StartArray();

		for(Handle<GraphEdge>& edge : node.getEdges())
		{
			GraphEdge* edgeInstance = edge.getInstance();

			Handle<GraphNode> toNode = edgeInstance -> traverse();

			if(!toNode.isValid()) continue;

			writer.StartObject();

			writer.Key("id");
			writer.Uint(edgeInstance -> getId());

			writeKeyString(writer, "to", toNode.getInstance() -> getName());

			writer.Key("actionFlags");
			writer.StartArray();
			for(std::string& flagName : hiveNames::actionFlagNames(edgeInstance -> getActionFlags()))
				writeString(writer, flagName);
			writer.EndArray();

			writer.Key("traversalKey");
			writer.Uint(edgeInstance -> getTraversalKey());

			writer.Key("actionsCompleteAfterTraverse");
			writer.Bool(edgeInstance -> getActionsCompleteAfterTraverse());

			writer.EndObject();
		}

		writer.EndArray();
	}

	std::string edgeJson(GraphEdge& edge)
	{
		rapidjson::StringBuffer buffer;
		JsonWriter writer(buffer);

		writer.StartObject();

		writer.Key("id");
		writer.Uint(edge.getId());

		Handle<GraphNode> toNode = edge.traverse();

		writeKeyString(writer, "to", toNode.isValid() ? toNode.getInstance() -> getName() : "");

		writer.Key("actionFlags");
		writer.StartArray();
		for(std::string& flagName : hiveNames::actionFlagNames(edge.getActionFlags())) writeString(writer, flagName);
		writer.EndArray();

		writer.Key("traversalKey");
		writer.Uint(edge.getTraversalKey());

		writer.Key("actionsCompleteAfterTraverse");
		writer.Bool(edge.getActionsCompleteAfterTraverse());

		writer.EndObject();

		return finish(buffer);
	}

	// Checks that every edge a node definition gives leads to a node that exists, or to the node itself.
	void checkEdgeTargets(Handle<GraphHive>& hive, const HiveNodeDescriptor& descriptor)
	{
		for(const HiveEdgeDescriptor& edge : descriptor.edges)
		{
			if(edge.toNodeName == descriptor.name) continue;

			if(!hive.getInstance() -> getNode(edge.toNodeName).isValid())
			{
				throw ApiException(ApiException::UNPROCESSABLE, "An edge leads to \"" + edge.toNodeName +
					"\", which is not a node in this hive.");
			}
		}
	}

	// Creates the edges a node definition gives, from a node already in its hive.
	void createEdges(Handle<GraphHive>& hive, Handle<GraphNode>& node, const HiveNodeDescriptor& descriptor)
	{
		for(const HiveEdgeDescriptor& edge : descriptor.edges)
		{
			Handle<GraphNode> toNode = edge.toNodeName == descriptor.name ? node :
				hive.getInstance() -> getNode(edge.toNodeName);

			if(!toNode.isValid())
			{
				throw ApiException(ApiException::UNPROCESSABLE, "An edge leads to \"" + edge.toNodeName +
					"\", which is not a node in this hive.");
			}

			HiveBuilder::createEdge(*node.getInstance(), toNode, edge);
		}
	}
}

ApiController::ApiController(HiveHost& host, GraphHiveCollection& collection) :
	_host{host}, _collection{collection}
{
}

ApiController::~ApiController()
{
}

const std::vector<ApiController::Route>& ApiController::__routes()
{
	static const std::vector<Route> routes =
	{
		{"GET", "/", &ApiController::__getIndex},
		{"GET", "/openapi.json", &ApiController::__getOpenApi},
		{"GET", "/hiveSchema.json", &ApiController::__getHiveSchema},
		{"GET", "/hives", &ApiController::__listHives},
		{"POST", "/hives", &ApiController::__createHive},
		{"GET", "/hives/{hive}", &ApiController::__getHive},
		{"DELETE", "/hives/{hive}", &ApiController::__deleteHive},
		{"GET", "/hives/{hive}/definition", &ApiController::__getHiveDefinition},
		{"GET", "/hives/{hive}/nodes", &ApiController::__listNodes},
		{"POST", "/hives/{hive}/nodes", &ApiController::__createNode},
		{"GET", "/hives/{hive}/nodes/{node}", &ApiController::__getNode},
		{"PATCH", "/hives/{hive}/nodes/{node}", &ApiController::__updateNode},
		{"DELETE", "/hives/{hive}/nodes/{node}", &ApiController::__deleteNode},
		{"POST", "/hives/{hive}/nodes/{node}/edges", &ApiController::__createEdge},
		{"DELETE", "/hives/{hive}/nodes/{node}/edges/{edgeId}", &ApiController::__deleteEdge},
		{"POST", "/hives/{hive}/nodes/{node}/poke", &ApiController::__pokeNode},
		{"POST", "/hives/{hive}/nodes/{node}/trigger", &ApiController::__triggerNode},
		{"POST", "/hives/{hive}/nodes/{node}/animate", &ApiController::__animateNode},
		{"PUT", "/hives/{hive}/strobeEmitters/{node}", &ApiController::__setStrobeEmitter},
		{"DELETE", "/hives/{hive}/strobeEmitters/{node}", &ApiController::__clearStrobeEmitter},
		{"PUT", "/hives/{hive}/strobeSurfaces/{surface}", &ApiController::__setStrobeSurface},
		{"DELETE", "/hives/{hive}/strobeSurfaces/{surface}", &ApiController::__clearStrobeSurface}
	};

	return routes;
}

std::vector<ApiController::RouteDescription> ApiController::getRoutes()
{
	std::vector<RouteDescription> descriptions;

	for(const Route& route : __routes()) descriptions.push_back(RouteDescription{route.method, route.pattern});

	return descriptions;
}

void ApiController::dispatch(HttpRequest& request, const std::string& path, HttpResponse& response)
{
	std::vector<std::string> segments = splitPath(path);

	std::string method = request.getMethod();

	std::vector<std::string> allowed;

	for(const Route& route : __routes())
	{
		Params params;

		if(!matchPattern(route.pattern, segments, params)) continue;

		if(method == route.method)
		{
			(this ->* route.handler)(params, request, response);
			return;
		}

		allowed.push_back(route.method);
	}

	if(allowed.empty()) throw ApiException(ApiException::NOT_FOUND, "Nothing is at " + path + ".");

	std::string allow;

	for(std::string& allowedMethod : allowed) allow += (allow.empty() ? "" : ", ") + allowedMethod;

	response.addHeader("Allow", allow);

	throw ApiException(ApiException::METHOD_NOT_ALLOWED, method + " is not allowed here. Allowed: " + allow + ".");
}

// -- Discovery --

void ApiController::__getIndex(Params& params, HttpRequest& request, HttpResponse& response)
{
	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();

	writeKeyString(writer, "name", "Matrytsya hive control API");
	writeKeyString(writer, "version", _API_VERSION);
	writeKeyString(writer, "basePath", _BASE_PATH);
	writeKeyString(writer, "openapi", std::string(_BASE_PATH) + "/openapi.json");
	writeKeyString(writer, "hiveSchema", std::string(_BASE_PATH) + "/hiveSchema.json");
	writeKeyString(writer, "agentGuide", apiDocs::AGENT_GUIDE);

	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

void ApiController::__getOpenApi(Params& params, HttpRequest& request, HttpResponse& response)
{
	respondJson(response, 200, apiDocs::OPENAPI_SPEC);
}

void ApiController::__getHiveSchema(Params& params, HttpRequest& request, HttpResponse& response)
{
	respondJson(response, 200, apiDocs::HIVE_SCHEMA);
}

// -- Hives --

void ApiController::__listHives(Params& params, HttpRequest& request, HttpResponse& response)
{
	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();

	writer.Key("hives");
	writer.StartArray();

	std::vector<std::string> names = _collection.getHiveNames();

	std::sort(names.begin(), names.end());

	for(std::string& name : names)
	{
		Handle<GraphHive> hive = _collection.getHive(name);

		if(!hive.isValid()) continue;

		writer.StartObject();

		writeKeyString(writer, "name", name);

		writer.Key("version");
		writer.Uint(hive.getInstance() -> getVersion());

		writer.Key("nodeCount");
		writer.Uint((unsigned) hive.getInstance() -> getNodeNames().size());

		writer.EndObject();
	}

	writer.EndArray();
	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

void ApiController::__createHive(Params& params, HttpRequest& request, HttpResponse& response)
{
	std::string body = request.getBody();

	if(body.find_first_not_of(" \t\r\n") == std::string::npos)
	{
		throw ApiException(ApiException::BAD_REQUEST, "A hive definition is required as the body.");
	}

	JsonHiveLoader loader(body);

	std::string hiveName = loader.getHiveName();

	if(_collection.getHive(hiveName).isValid())
	{
		throw ApiException(ApiException::CONFLICT, "A hive named \"" + hiveName + "\" is already running.");
	}

	std::string persistParam = request.getQueryParam("persist");

	bool persist = persistParam != "false";

	// Storing it would overwrite a stored hive of the same name that is not running, which is only ever done
	// on purpose, by deleting that one with purge first.
	if(persist && _host.isStored(hiveName))
	{
		throw ApiException(ApiException::CONFLICT, "A hive named \"" + hiveName + "\" is stored in the hive "
			"database but not running. Delete it with purge=true first, or create this one with persist=false.");
	}

	Handle<GraphHive> hive = _host.addHive(loader, false, persist);

	response.addHeader("Location", std::string(_BASE_PATH) + "/hives/" + encodeSegment(hiveName));

	respondJson(response, 201, __hiveSummary(hive));
}

void ApiController::__getHive(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);

	respondJson(response, 200, __hiveSummary(hive));
}

void ApiController::__deleteHive(Params& params, HttpRequest& request, HttpResponse& response)
{
	std::string hiveName = params["hive"];

	bool purge = request.getQueryParam("purge") == "true";

	bool removed = _host.removeHive(hiveName, purge);

	bool purged = removed && purge && _host.hasStore();

	// A hive left in the database by an earlier delete without purge is not running, but can still be purged.
	if(!removed && purge) purged = _host.purgeStored(hiveName);

	if(!removed && !purged)
	{
		throw ApiException(ApiException::NOT_FOUND, "No hive named \"" + hiveName + "\" is running" +
			(purge ? " or stored." : "."));
	}

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();

	writeKeyString(writer, "name", hiveName);

	writer.Key("deleted");
	writer.Bool(removed);

	writer.Key("purged");
	writer.Bool(purged);

	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

void ApiController::__getHiveDefinition(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<HiveRecorder> recorder = __findRecorder(params["hive"]);

	respondJson(response, 200, JsonHiveWriter::hiveDocument(recorder.getInstance() -> snapshot()));
}

// -- Nodes --

void ApiController::__listNodes(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<HiveRecorder> recorder = __findRecorder(params["hive"]);

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();

	writer.Key("nodes");
	writer.StartArray();

	for(std::string& name : hive.getInstance() -> getNodeNames())
	{
		Handle<GraphNode> node = hive.getInstance() -> getNode(name);

		if(!node.isValid()) continue;

		HiveNodeDescriptor descriptor;

		std::string typeName = recorder.getInstance() -> getNode(name, descriptor) ?
			hiveNames::nodeTypeName(descriptor.type) : GraphNode::typeName(node.getInstance() -> getType());

		writer.StartObject();

		writeKeyString(writer, "name", name);

		writer.Key("id");
		writer.Uint(node.getInstance() -> getId());

		writeKeyString(writer, "type", typeName);

		writeEdges(writer, *node.getInstance());

		writer.EndObject();
	}

	writer.EndArray();
	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

void ApiController::__createNode(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<HiveRecorder> recorder = __findRecorder(params["hive"]);

	rapidjson::Document document;

	parseBody(request, document, false);

	HiveNodeDescriptor descriptor = JsonHiveLoader::parseNodeFragment(toJson(document));

	if(hive.getInstance() -> getNode(descriptor.name).isValid())
	{
		throw ApiException(ApiException::CONFLICT, "A node named \"" + descriptor.name + "\" already exists.");
	}

	checkEdgeTargets(hive, descriptor);

	GraphNode* node = HiveBuilder::createNode(descriptor);

	// The hive manages the node's initial reference count from here, whether or not it joins.
	if(!hive.getInstance() -> addUniquelyNamedNode(node))
	{
		throw ApiException(ApiException::CONFLICT, "A node named \"" + descriptor.name + "\" already exists.");
	}

	Handle<GraphNode> nodeHandle = hive.getInstance() -> getNode(descriptor.name);

	try
	{
		createEdges(hive, nodeHandle, descriptor);
	}
	catch(...)
	{
		// Taken out again, so that a failed request leaves the hive as it was.
		hive.getInstance() -> removeNode(nodeHandle);
		throw;
	}

	response.addHeader("Location", std::string(_BASE_PATH) + "/hives/" + encodeSegment(params["hive"]) + "/nodes/" +
		encodeSegment(descriptor.name));

	respondJson(response, 201, __nodeView(recorder, nodeHandle));
}

void ApiController::__getNode(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<HiveRecorder> recorder = __findRecorder(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	respondJson(response, 200, __nodeView(recorder, node));
}

void ApiController::__updateNode(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<HiveRecorder> recorder = __findRecorder(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	std::string nodeName = params["node"];

	rapidjson::Document patch;

	parseBody(request, patch, false);

	__checkNotBoundToSurface(recorder, nodeName);

	HiveNodeDescriptor current;

	if(!recorder.getInstance() -> getNode(nodeName, current))
	{
		throw ApiException(ApiException::NOT_FOUND, "No node named \"" + nodeName + "\" is in this hive.");
	}

	rapidjson::Document merged;

	std::string currentJson = JsonHiveWriter::nodeFragment(current);

	merged.Parse(currentJson.c_str(), currentJson.size());

	for(rapidjson::Value::MemberIterator member = patch.MemberBegin(); member != patch.MemberEnd(); member++)
	{
		std::string memberName(member -> name.GetString(), member -> name.GetStringLength());

		if((memberName == "name" || memberName == "type") &&
			!(member -> value.IsString() && merged.HasMember(memberName.c_str())
				&& merged[memberName.c_str()] == member -> value))
		{
			throw ApiException(ApiException::UNPROCESSABLE, "A node's \"" + memberName + "\" cannot be changed. "
				"Delete the node and create a new one instead.");
		}

		if(merged.HasMember(member -> name)) merged.RemoveMember(member -> name);

		rapidjson::Value name(member -> name, merged.GetAllocator());
		rapidjson::Value value(member -> value, merged.GetAllocator());

		merged.AddMember(name, value, merged.GetAllocator());
	}

	HiveNodeDescriptor descriptor = JsonHiveLoader::parseNodeFragment(toJson(merged));

	bool replaceEdges = patch.HasMember("edges");

	if(replaceEdges) checkEdgeTargets(hive, descriptor);

	GraphNode* replacement = HiveBuilder::createNode(descriptor);

	// The hive manages the replacement's initial reference count from here, whether or not it joins.
	if(!hive.getInstance() -> replaceNode(node, replacement))
	{
		throw ApiException(ApiException::CONFLICT, "The node \"" + nodeName + "\" changed while it was being "
			"updated. Read it again and retry.");
	}

	Handle<GraphNode> replacementHandle = hive.getInstance() -> getNode(nodeName);

	if(replaceEdges && replacementHandle.isValid())
	{
		for(Handle<GraphEdge>& edge : replacementHandle.getInstance() -> getEdges())
		{
			replacementHandle.getInstance() -> removeEdge(edge);
		}

		createEdges(hive, replacementHandle, descriptor);
	}

	respondJson(response, 200, __nodeView(recorder, replacementHandle));
}

void ApiController::__deleteNode(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<HiveRecorder> recorder = __findRecorder(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	std::string nodeName = params["node"];

	__checkNotBoundToSurface(recorder, nodeName);

	if(hive.getInstance() -> getNodeNames().size() <= 1)
	{
		throw ApiException(ApiException::CONFLICT, "A hive must keep at least one node. Delete the hive instead.");
	}

	if(!hive.getInstance() -> removeNode(node))
	{
		throw ApiException(ApiException::NOT_FOUND, "No node named \"" + nodeName + "\" is in this hive.");
	}

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();

	writeKeyString(writer, "name", nodeName);

	writer.Key("deleted");
	writer.Bool(true);

	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

// -- Edges --

void ApiController::__createEdge(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	rapidjson::Document document;

	parseBody(request, document, false);

	HiveEdgeDescriptor descriptor;

	descriptor.toNodeName = optionalString(document, "to");

	if(descriptor.toNodeName.empty())
	{
		throw ApiException(ApiException::UNPROCESSABLE, "\"to\" must name the node the edge leads to.");
	}

	if(document.HasMember("actionFlags"))
	{
		if(!document["actionFlags"].IsArray())
		{
			throw ApiException(ApiException::UNPROCESSABLE, "\"actionFlags\" must be an array of action flag names.");
		}

		for(rapidjson::Value& flag : document["actionFlags"].GetArray())
		{
			if(!flag.IsString())
			{
				throw ApiException(ApiException::UNPROCESSABLE,
					"\"actionFlags\" must be an array of action flag names.");
			}

			descriptor.actionFlagNames.push_back(std::string(flag.GetString(), flag.GetStringLength()));
		}
	}

	if(document.HasMember("traversalKey"))
	{
		if(!document["traversalKey"].IsUint())
		{
			throw ApiException(ApiException::UNPROCESSABLE, "\"traversalKey\" must be a whole number, 0 for none.");
		}

		descriptor.traversalKey = document["traversalKey"].GetUint();
	}

	if(document.HasMember("actionsCompleteAfterTraverse"))
	{
		if(!document["actionsCompleteAfterTraverse"].IsBool())
		{
			throw ApiException(ApiException::UNPROCESSABLE, "\"actionsCompleteAfterTraverse\" must be true or false.");
		}

		descriptor.actionsCompleteAfterTraverse = document["actionsCompleteAfterTraverse"].GetBool();
	}

	Handle<GraphNode> toNode = __findNode(hive, descriptor.toNodeName);

	Handle<GraphEdge> edge = HiveBuilder::createEdge(*node.getInstance(), toNode, descriptor);

	respondJson(response, 201, edgeJson(*edge.getInstance()));
}

void ApiController::__deleteEdge(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	const std::string& edgeIdText = params["edgeId"];

	char* end = 0;

	unsigned long edgeId = strtoul(edgeIdText.c_str(), &end, 10);

	Handle<GraphEdge> edge(0);

	if(!edgeIdText.empty() && *end == '\0' && isdigit((unsigned char) edgeIdText[0]))
	{
		edge = node.getInstance() -> getEdge((unsigned) edgeId);
	}

	if(!edge.isValid())
	{
		throw ApiException(ApiException::NOT_FOUND, "The node \"" + params["node"] + "\" has no edge with id " +
			edgeIdText + ".");
	}

	node.getInstance() -> removeEdge(edge);

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();

	writer.Key("id");
	writer.Uint(edge.getInstance() -> getId());

	writer.Key("deleted");
	writer.Bool(true);

	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

// -- Runtime control --

void ApiController::__pokeNode(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	rapidjson::Document document;

	parseBody(request, document, true);

	std::string typeName = optionalString(document, "type");

	GraphPoke::PokeType type;

	if(typeName.empty() || typeName == "hit") type = GraphPoke::PokeType::HIT;
	else if(typeName == "grab") type = GraphPoke::PokeType::GRAB;
	else if(typeName == "drag") type = GraphPoke::PokeType::DRAG;
	else if(typeName == "hoverEnter") type = GraphPoke::PokeType::HOVER_ENTER;
	else if(typeName == "hoverLeave") type = GraphPoke::PokeType::HOVER_LEAVE;
	else
	{
		throw ApiException(ApiException::UNPROCESSABLE, "\"type\" must be one of hit, grab, drag, hoverEnter or "
			"hoverLeave.");
	}

	unsigned chunkId = 0;

	if(document.HasMember("chunkId"))
	{
		if(!document["chunkId"].IsUint())
		{
			throw ApiException(ApiException::UNPROCESSABLE, "\"chunkId\" must be a whole number, 0 for none.");
		}

		chunkId = document["chunkId"].GetUint();
	}

	GraphPoke::PokeData data{};

	hive.getInstance() -> poke(node.getInstance() -> getId(), GraphPoke(type, data, chunkId));

	respondJson(response, 202, "{\"accepted\":true}");
}

void ApiController::__triggerNode(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	rapidjson::Document document;

	parseBody(request, document, true);

	std::string targetName = optionalString(document, "nodeName");
	std::string targetType = optionalString(document, "nodeType");

	if(!targetType.empty())
	{
		std::vector<std::string> typeNames = GraphNode::typeNames();

		if(std::find(typeNames.begin(), typeNames.end(), targetType) == typeNames.end())
		{
			std::string known;

			for(std::string& typeName : typeNames) known += (known.empty() ? "" : ", ") + typeName;

			throw ApiException(ApiException::UNPROCESSABLE, "\"nodeType\" must be one of " + known + ".");
		}
	}

	std::vector<ModelToolCallParameterValue> parameters;

	if(!targetName.empty()) parameters.push_back(ModelToolCallParameterValue("nodeName", targetName));
	if(!targetType.empty()) parameters.push_back(ModelToolCallParameterValue("nodeType", targetType));

	// The same trigger a model's tool call emits, so that the two cannot drift apart in what they do.
	TriggerEmitterToolBindings* bindings = new TriggerEmitterToolBindings(node);

	Handle<TriggerEmitterToolBindings> bindingsHandle(bindings);

	// The handle holds the reference; release the implicit construction ref.
	bindings -> decrRef();

	bindings -> processBinding("emitTrigger", parameters);

	respondJson(response, 202, "{\"accepted\":true}");
}

void ApiController::__animateNode(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	rapidjson::Document document;

	parseBody(request, document, false);

	if(!document.HasMember("animating") || !document["animating"].IsBool())
	{
		throw ApiException(ApiException::UNPROCESSABLE, "\"animating\" must be true or false.");
	}

	bool animating = document["animating"].GetBool();

	AnimateActionTarget* target = node.getInstance() -> getAnimateActionTarget();

	if(!target)
	{
		throw ApiException(ApiException::UNPROCESSABLE, "The node \"" + params["node"] + "\" cannot be animated.");
	}

	// A serial of zero always applies, whatever serial the node last saw.
	target -> setAnimating(animating, 0);

	respondJson(response, 200, std::string("{\"animating\":") + (animating ? "true" : "false") + "}");
}

void ApiController::__setStrobeEmitter(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	rapidjson::Document document;

	parseBody(request, document, false);

	unsigned periodMs = requiredPeriod(document);

	StrobeEmitterNode* emitter = node.getInstance() -> asStrobeEmitter();

	if(!emitter)
	{
		throw ApiException(ApiException::UNPROCESSABLE, "The node \"" + params["node"] + "\" cannot be a strobe "
			"emitter. Only a SceneRootNode can.");
	}

	hive.getInstance() -> setStrobeEmitter(Handle<StrobeEmitterNode>(emitter), periodMs);

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();
	writeKeyString(writer, "nodeName", params["node"]);
	writer.Key("periodMs");
	writer.Uint(periodMs);
	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

void ApiController::__clearStrobeEmitter(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);
	Handle<GraphNode> node = __findNode(hive, params["node"]);

	StrobeEmitterNode* emitter = node.getInstance() -> asStrobeEmitter();

	if(emitter) hive.getInstance() -> clearStrobeEmitter(Handle<StrobeEmitterNode>(emitter));

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();
	writeKeyString(writer, "nodeName", params["node"]);
	writer.Key("strobing");
	writer.Bool(false);
	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

void ApiController::__setStrobeSurface(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);

	Handle<GraphHiveSurface> surface = hive.getInstance() -> getSurface(params["surface"]);

	if(!surface.isValid())
	{
		throw ApiException(ApiException::NOT_FOUND, "No surface named \"" + params["surface"] + "\" is in this hive.");
	}

	rapidjson::Document document;

	parseBody(request, document, false);

	unsigned periodMs = requiredPeriod(document);

	hive.getInstance() -> setStrobeSurface(surface, periodMs);

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();
	writeKeyString(writer, "surfaceName", params["surface"]);
	writer.Key("periodMs");
	writer.Uint(periodMs);
	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

void ApiController::__clearStrobeSurface(Params& params, HttpRequest& request, HttpResponse& response)
{
	Handle<GraphHive> hive = __findHive(params["hive"]);

	Handle<GraphHiveSurface> surface = hive.getInstance() -> getSurface(params["surface"]);

	if(!surface.isValid())
	{
		throw ApiException(ApiException::NOT_FOUND, "No surface named \"" + params["surface"] + "\" is in this hive.");
	}

	hive.getInstance() -> clearStrobeSurface(surface);

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();
	writeKeyString(writer, "surfaceName", params["surface"]);
	writer.Key("strobing");
	writer.Bool(false);
	writer.EndObject();

	respondJson(response, 200, finish(buffer));
}

// -- Helpers --

Handle<GraphHive> ApiController::__findHive(const std::string& hiveName)
{
	Handle<GraphHive> hive = _collection.getHive(hiveName);

	// Only a hive this host hosts is offered, as only for one of those is there a record of what it is made of.
	if(!hive.isValid() || !_host.getRecorder(hiveName).isValid())
	{
		throw ApiException(ApiException::NOT_FOUND, "No hive named \"" + hiveName + "\" is running.");
	}

	return hive;
}

Handle<HiveRecorder> ApiController::__findRecorder(const std::string& hiveName)
{
	Handle<HiveRecorder> recorder = _host.getRecorder(hiveName);

	if(!recorder.isValid())
		throw ApiException(ApiException::NOT_FOUND, "No hive named \"" + hiveName + "\" is running.");

	return recorder;
}

Handle<GraphNode> ApiController::__findNode(Handle<GraphHive>& hive, const std::string& nodeName)
{
	Handle<GraphNode> node = hive.getInstance() -> getNode(nodeName);

	if(!node.isValid())
		throw ApiException(ApiException::NOT_FOUND, "No node named \"" + nodeName + "\" is in this hive.");

	return node;
}

void ApiController::__checkNotBoundToSurface(Handle<HiveRecorder>& recorder, const std::string& nodeName)
{
	for(HiveSurfaceDescriptor& surface : recorder.getInstance() -> snapshot().surfaces)
	{
		if(surface.sceneRootNodeName == nodeName || surface.initialFocusNodeName == nodeName)
		{
			throw ApiException(ApiException::CONFLICT, "The surface \"" + surface.name + "\" is bound to the node \"" +
				nodeName + "\", so the node cannot be changed or deleted while the hive runs.");
		}
	}
}

std::string ApiController::__hiveSummary(Handle<GraphHive>& hive)
{
	GraphHive* hiveInstance = hive.getInstance();
	std::string hiveName = hiveInstance -> getName();

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();

	writeKeyString(writer, "name", hiveName);

	writer.Key("version");
	writer.Uint(hiveInstance -> getVersion());

	writer.Key("nodeCount");
	writer.Uint((unsigned) hiveInstance -> getNodeNames().size());

	writer.Key("persisted");
	writer.Bool(_host.getPersisted(hiveName));

	writer.Key("surfaces");
	writer.StartArray();

	for(std::pair<std::string, std::string>& surface : _host.getSurfacePaths(hiveName))
	{
		writer.StartObject();
		writeKeyString(writer, "name", surface.first);
		writeKeyString(writer, "path", surface.second);
		writer.EndObject();
	}

	writer.EndArray();

	writer.Key("strobeEmitters");
	writer.StartArray();

	for(GraphHive::StrobeRegistration& emitter : hiveInstance -> getStrobeEmitters())
	{
		writer.StartObject();
		writeKeyString(writer, "nodeName", emitter.name);
		writer.Key("periodMs");
		writer.Uint(emitter.periodMs);
		writer.EndObject();
	}

	writer.EndArray();

	writer.Key("strobeSurfaces");
	writer.StartArray();

	for(GraphHive::StrobeRegistration& surface : hiveInstance -> getStrobeSurfaces())
	{
		writer.StartObject();
		writeKeyString(writer, "surfaceName", surface.name);
		writer.Key("periodMs");
		writer.Uint(surface.periodMs);
		writer.EndObject();
	}

	writer.EndArray();

	writer.EndObject();

	return finish(buffer);
}

std::string ApiController::__nodeView(Handle<HiveRecorder>& recorder, Handle<GraphNode>& node)
{
	if(!node.isValid()) throw ApiException(ApiException::NOT_FOUND, "The node is no longer in this hive.");

	std::string nodeName = node.getInstance() -> getName();

	HiveNodeDescriptor descriptor;

	if(!recorder.getInstance() -> getNode(nodeName, descriptor))
	{
		throw ApiException(ApiException::NOT_FOUND, "No node named \"" + nodeName + "\" is in this hive.");
	}

	std::string definition = JsonHiveWriter::nodeFragment(descriptor);

	rapidjson::StringBuffer buffer;
	JsonWriter writer(buffer);

	writer.StartObject();

	writeKeyString(writer, "name", nodeName);

	writer.Key("id");
	writer.Uint(node.getInstance() -> getId());

	writeKeyString(writer, "type", hiveNames::nodeTypeName(descriptor.type));

	writer.Key("definition");
	writer.RawValue(definition.c_str(), definition.size(), rapidjson::kObjectType);

	writeEdges(writer, *node.getInstance());

	writer.EndObject();

	return finish(buffer);
}
