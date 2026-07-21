#include "JsonHiveLoader.hpp"

#include "../PersistException.hpp"
#include "../../graph/GraphNodeLocation.hpp"
#include "../../graph/graphSceneElements.hpp"
#include "../../rapidjson/document.h"

namespace
{
	// -- Parsing helpers, used only while JsonHiveLoader's constructor extracts everything up front --

	HiveNodeDescriptor::Type __nodeTypeFromString(const std::string& type)
	{
		if(type == "PingNode") return HiveNodeDescriptor::PING;
		if(type == "TeleportNode") return HiveNodeDescriptor::TELEPORT;
		if(type == "SceneRootNode") return HiveNodeDescriptor::SCENE_ROOT;
		if(type == "SceneGeometryNode") return HiveNodeDescriptor::SCENE_GEOMETRY;
		if(type == "SceneGeometryScriptNode") return HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT;
		if(type == "SceneTransformNode") return HiveNodeDescriptor::SCENE_TRANSFORM;
		if(type == "SceneTransformScriptNode") return HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT;

		throw PersistException(PersistException::UNKNOWN_NODE_TYPE);
	}

	bool __isScriptType(HiveNodeDescriptor::Type type)
	{
		return type == HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT || type == HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT;
	}

	HiveSurfaceDescriptor::Type __surfaceTypeFromString(const std::string& type)
	{
		if(type == "GraphHiveSceneSurface") return HiveSurfaceDescriptor::SCENE_SURFACE;

		throw PersistException(PersistException::UNKNOWN_SURFACE_TYPE);
	}

	void __readDoubleArray(const rapidjson::Value& arrayValue, double* out, unsigned count, PersistException::Error onError)
	{
		if(!arrayValue.IsArray() || arrayValue.Size() != count) throw PersistException(onError);

		for(unsigned i = 0; i < count; i++)
		{
			if(!arrayValue[i].IsNumber()) throw PersistException(onError);

			out[i] = arrayValue[i].GetDouble();
		}
	}

	void __parseEdges(const rapidjson::Value& nodeValue, HiveNodeDescriptor& descriptor)
	{
		if(!nodeValue.HasMember("edges")) return;

		const rapidjson::Value& edgesValue = nodeValue["edges"];

		if(!edgesValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_EDGES);

		for(auto& edgeValue : edgesValue.GetArray())
		{
			if(!edgeValue.IsObject() || !edgeValue.HasMember("toNodeName") || !edgeValue["toNodeName"].IsString())
			{
				throw PersistException(PersistException::JSON_INVALID_EDGES);
			}

			HiveEdgeDescriptor edgeDescriptor;
			edgeDescriptor.toNodeName = edgeValue["toNodeName"].GetString();

			if(edgeValue.HasMember("actionFlags"))
			{
				const rapidjson::Value& flagsValue = edgeValue["actionFlags"];

				if(!flagsValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_EDGES);

				for(auto& flagValue : flagsValue.GetArray())
				{
					if(!flagValue.IsString()) throw PersistException(PersistException::JSON_INVALID_EDGES);

					edgeDescriptor.actionFlagNames.push_back(flagValue.GetString());
				}
			}

			descriptor.edges.push_back(edgeDescriptor);
		}
	}

	void __parseNotifySources(const rapidjson::Value& nodeValue, HiveNodeDescriptor& descriptor)
	{
		if(!nodeValue.HasMember("notifySources")) return;

		const rapidjson::Value& sourcesValue = nodeValue["notifySources"];

		if(!sourcesValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_NOTIFY_SOURCES);

		for(auto& sourceValue : sourcesValue.GetArray())
		{
			if(!sourceValue.IsString()) throw PersistException(PersistException::JSON_INVALID_NOTIFY_SOURCES);

			descriptor.notifySourceNames.push_back(sourceValue.GetString());
		}
	}

	GraphNodeLocation __parseNodeLocation(const rapidjson::Value& locationValue)
	{
		if(!locationValue.IsObject()) throw PersistException(PersistException::JSON_INVALID_DESTINATION);

		if(!locationValue.HasMember("hiveName") || !locationValue["hiveName"].IsString() ||
			!locationValue.HasMember("nodeName") || !locationValue["nodeName"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_DESTINATION);
		}

		GraphNodeLocation location;

		location.setHiveName(locationValue["hiveName"].GetString());
		location.setNodeName(locationValue["nodeName"].GetString());

		if(locationValue.HasMember("hostname"))
		{
			if(!locationValue["hostname"].IsString()) throw PersistException(PersistException::JSON_INVALID_DESTINATION);

			location.setHostname(locationValue["hostname"].GetString());
		}
		else
		{
			// Schema default.
			location.setHostname("localhost");
		}

		if(locationValue.HasMember("port"))
		{
			if(!locationValue["port"].IsInt()) throw PersistException(PersistException::JSON_INVALID_DESTINATION);

			location.setPort(locationValue["port"].GetInt());
		}
		else
		{
			location.setPort(0);
		}

		return location;
	}

	std::vector<Vertex> __parseVertexes(const rapidjson::Value& nodeValue)
	{
		std::vector<Vertex> vertexes;

		if(!nodeValue.HasMember("vertexes")) return vertexes;

		const rapidjson::Value& vertexesValue = nodeValue["vertexes"];

		if(!vertexesValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_VERTEXES);

		for(auto& vertexValue : vertexesValue.GetArray())
		{
			if(!vertexValue.IsObject() || !vertexValue.HasMember("posn"))
			{
				throw PersistException(PersistException::JSON_INVALID_VERTEXES);
			}

			// Vertex is a plain POD struct with no member initialisers, so it must be zeroed before
			// filling in whichever fields the JSON actually supplies.
			Vertex vertex{};

			__readDoubleArray(vertexValue["posn"], vertex.posn, 3, PersistException::JSON_INVALID_VERTEXES);

			if(vertexValue.HasMember("colour"))
			{
				const rapidjson::Value& colourValue = vertexValue["colour"];

				if(!colourValue.IsArray() || colourValue.Size() != 4)
				{
					throw PersistException(PersistException::JSON_INVALID_VERTEXES);
				}

				for(unsigned i = 0; i < 4; i++)
				{
					if(!colourValue[i].IsInt()) throw PersistException(PersistException::JSON_INVALID_VERTEXES);

					int component = colourValue[i].GetInt();

					if(component < 0 || component > 255) throw PersistException(PersistException::JSON_INVALID_VERTEXES);

					vertex.colour[i] = static_cast<std::byte>(component);
				}
			}

			if(vertexValue.HasMember("texCoords"))
			{
				__readDoubleArray(vertexValue["texCoords"], vertex.texCoords, 2, PersistException::JSON_INVALID_VERTEXES);
			}

			if(vertexValue.HasMember("normal"))
			{
				__readDoubleArray(vertexValue["normal"], vertex.normal, 3, PersistException::JSON_INVALID_VERTEXES);
			}

			vertexes.push_back(vertex);
		}

		return vertexes;
	}

	void __parseScriptSource(const rapidjson::Value& nodeValue, std::string& coreScript, std::string& pokeScript)
	{
		if(!nodeValue.HasMember("coreScript") || !nodeValue["coreScript"].IsString() ||
			!nodeValue.HasMember("pokeScript") || !nodeValue["pokeScript"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_SCRIPT_SOURCE);
		}

		coreScript = nodeValue["coreScript"].GetString();
		pokeScript = nodeValue["pokeScript"].GetString();
	}

	HiveNodeDescriptor __parseNode(const rapidjson::Value& nodeValue)
	{
		if(!nodeValue.IsObject() || !nodeValue.HasMember("type") || !nodeValue["type"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_NODE_BASE);
		}

		HiveNodeDescriptor descriptor{};

		// May throw UNKNOWN_NODE_TYPE if "type" is a string but not one of the recognised values.
		descriptor.type = __nodeTypeFromString(nodeValue["type"].GetString());

		if(!nodeValue.HasMember("name") || !nodeValue["name"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_NODE_BASE);
		}

		descriptor.name = nodeValue["name"].GetString();

		if(nodeValue.HasMember("pokeEnabled"))
		{
			if(!nodeValue["pokeEnabled"].IsBool()) throw PersistException(PersistException::JSON_INVALID_POKE_ENABLED);

			descriptor.pokeEnabled = nodeValue["pokeEnabled"].GetBool();
		}

		__parseEdges(nodeValue, descriptor);
		__parseNotifySources(nodeValue, descriptor);

		if(descriptor.type == HiveNodeDescriptor::TELEPORT)
		{
			if(!nodeValue.HasMember("destination")) throw PersistException(PersistException::JSON_INVALID_DESTINATION);

			descriptor.destination = __parseNodeLocation(nodeValue["destination"]);
		}

		if(descriptor.type == HiveNodeDescriptor::SCENE_GEOMETRY ||
			descriptor.type == HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT)
		{
			descriptor.hasVertexes = nodeValue.HasMember("vertexes");
			descriptor.vertexes = __parseVertexes(nodeValue);
		}

		if(descriptor.type == HiveNodeDescriptor::SCENE_TRANSFORM ||
			descriptor.type == HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT)
		{
			descriptor.hasTransform = nodeValue.HasMember("transform");

			if(descriptor.hasTransform)
			{
				__readDoubleArray(nodeValue["transform"], descriptor.transform, 16, PersistException::JSON_INVALID_TRANSFORM);
			}
		}

		if(__isScriptType(descriptor.type))
		{
			__parseScriptSource(nodeValue, descriptor.coreScript, descriptor.pokeScript);
		}

		return descriptor;
	}

	HiveSurfaceDescriptor __parseSurface(const rapidjson::Value& surfaceValue)
	{
		if(!surfaceValue.IsObject() || !surfaceValue.HasMember("type") || !surfaceValue["type"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_SURFACES);
		}

		HiveSurfaceDescriptor descriptor{};

		// May throw UNKNOWN_SURFACE_TYPE if "type" is a string but not one of the recognised values.
		descriptor.type = __surfaceTypeFromString(surfaceValue["type"].GetString());

		if(!surfaceValue.HasMember("name") || !surfaceValue["name"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_SURFACES);
		}

		descriptor.name = surfaceValue["name"].GetString();

		if(descriptor.type == HiveSurfaceDescriptor::SCENE_SURFACE)
		{
			if(!surfaceValue.HasMember("sceneRootNodeName") || !surfaceValue["sceneRootNodeName"].IsString())
			{
				throw PersistException(PersistException::JSON_INVALID_SURFACES);
			}

			descriptor.sceneRootNodeName = surfaceValue["sceneRootNodeName"].GetString();

			if(surfaceValue.HasMember("initialFocusNodeName"))
			{
				if(!surfaceValue["initialFocusNodeName"].IsString())
				{
					throw PersistException(PersistException::JSON_INVALID_SURFACE_INITIAL_FOCUS_NODE_NAME);
				}

				descriptor.initialFocusNodeName = surfaceValue["initialFocusNodeName"].GetString();
			}

			if(surfaceValue.HasMember("focusViewportFraction"))
			{
				if(!surfaceValue["focusViewportFraction"].IsNumber() || surfaceValue["focusViewportFraction"].GetDouble() <= 0.0)
				{
					throw PersistException(PersistException::JSON_INVALID_SURFACE_FOCUS_VIEWPORT_FRACTION);
				}

				descriptor.focusViewportFraction = surfaceValue["focusViewportFraction"].GetDouble();
			}
		}

		if(surfaceValue.HasMember("default"))
		{
			if(!surfaceValue["default"].IsBool()) throw PersistException(PersistException::JSON_INVALID_SURFACE_DEFAULT);

			descriptor.isDefault = surfaceValue["default"].GetBool();
		}

		return descriptor;
	}
}

JsonHiveLoader::JsonHiveLoader(const std::string& json)
{
	rapidjson::Document document;

	document.Parse(json.c_str());

	if(document.HasParseError()) throw PersistException(PersistException::JSON_PARSE_ERROR);

	if(!document.IsObject()) throw PersistException(PersistException::JSON_ROOT_NOT_OBJECT);

	if(!document.HasMember("name") || !document["name"].IsString())
	{
		throw PersistException(PersistException::JSON_INVALID_NAME);
	}

	_hiveName = document["name"].GetString();

	if(_hiveName.empty() || _hiveName.length() > 128)
	{
		throw PersistException(PersistException::JSON_INVALID_NAME);
	}

	if(!document.HasMember("nodes") || !document["nodes"].IsArray())
	{
		throw PersistException(PersistException::JSON_INVALID_NODES);
	}

	// An empty "nodes" array is accepted here; requiring at least one node is HiveBuilder's concern.
	for(auto& nodeValue : document["nodes"].GetArray())
	{
		_nodes.push_back(__parseNode(nodeValue));
	}

	if(document.HasMember("surfaces"))
	{
		const rapidjson::Value& surfacesValue = document["surfaces"];

		if(!surfacesValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_SURFACES);

		for(auto& surfaceValue : surfacesValue.GetArray())
		{
			_surfaces.push_back(__parseSurface(surfaceValue));
		}
	}

	if(document.HasMember("strobeEmitters"))
	{
		const rapidjson::Value& strobeEmittersValue = document["strobeEmitters"];

		if(!strobeEmittersValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_STROBE_EMITTERS);

		for(auto& strobeEmitterValue : strobeEmittersValue.GetArray())
		{
			if(!strobeEmitterValue.IsObject() ||
				!strobeEmitterValue.HasMember("nodeName") || !strobeEmitterValue["nodeName"].IsString() ||
				(strobeEmitterValue.HasMember("periodMs") && !strobeEmitterValue["periodMs"].IsUint()))
			{
				throw PersistException(PersistException::JSON_INVALID_STROBE_EMITTERS);
			}

			unsigned periodMs = strobeEmitterValue.HasMember("periodMs") ?
				strobeEmitterValue["periodMs"].GetUint() : 33; // Schema default.

			_strobeEmitters.emplace_back(strobeEmitterValue["nodeName"].GetString(), periodMs);
		}
	}

	if(document.HasMember("strobeSurfaces"))
	{
		const rapidjson::Value& strobeSurfacesValue = document["strobeSurfaces"];

		if(!strobeSurfacesValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_STROBE_SURFACES);

		for(auto& strobeSurfaceValue : strobeSurfacesValue.GetArray())
		{
			if(!strobeSurfaceValue.IsObject() ||
				!strobeSurfaceValue.HasMember("surfaceName") || !strobeSurfaceValue["surfaceName"].IsString() ||
				(strobeSurfaceValue.HasMember("periodMs") && !strobeSurfaceValue["periodMs"].IsUint()))
			{
				throw PersistException(PersistException::JSON_INVALID_STROBE_SURFACES);
			}

			unsigned periodMs = strobeSurfaceValue.HasMember("periodMs") ?
				strobeSurfaceValue["periodMs"].GetUint() : 33; // Schema default.

			_strobeSurfaces.emplace_back(strobeSurfaceValue["surfaceName"].GetString(), periodMs);
		}
	}
}

JsonHiveLoader::~JsonHiveLoader()
{
}

std::string JsonHiveLoader::getHiveName()
{
	return _hiveName;
}

unsigned JsonHiveLoader::getNodeCount()
{
	return _nodes.size();
}

HiveNodeDescriptor JsonHiveLoader::getNode(unsigned index)
{
	return _nodes[index];
}

unsigned JsonHiveLoader::getSurfaceCount()
{
	return _surfaces.size();
}

HiveSurfaceDescriptor JsonHiveLoader::getSurface(unsigned index)
{
	return _surfaces[index];
}

unsigned JsonHiveLoader::getStrobeEmitterCount()
{
	return _strobeEmitters.size();
}

void JsonHiveLoader::getStrobeEmitter(unsigned index, std::string& nodeName, unsigned& periodMs)
{
	nodeName = _strobeEmitters[index].first;
	periodMs = _strobeEmitters[index].second;
}

unsigned JsonHiveLoader::getStrobeSurfaceCount()
{
	return _strobeSurfaces.size();
}

void JsonHiveLoader::getStrobeSurface(unsigned index, std::string& surfaceName, unsigned& periodMs)
{
	surfaceName = _strobeSurfaces[index].first;
	periodMs = _strobeSurfaces[index].second;
}
