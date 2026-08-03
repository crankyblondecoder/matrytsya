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
		if(type == "AgentNode") return HiveNodeDescriptor::AGENT;
		if(type == "TriggerNode") return HiveNodeDescriptor::TRIGGER;

		throw PersistException(PersistException::UNKNOWN_NODE_TYPE);
	}

	bool __isScriptType(HiveNodeDescriptor::Type type)
	{
		return type == HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT ||
			type == HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT ||
			type == HiveNodeDescriptor::TRIGGER;
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

			if(edgeValue.HasMember("actionsCompleteAfterTraverse"))
			{
				if(!edgeValue["actionsCompleteAfterTraverse"].IsBool()) throw PersistException(PersistException::JSON_INVALID_EDGES);

				edgeDescriptor.actionsCompleteAfterTraverse = edgeValue["actionsCompleteAfterTraverse"].GetBool();
			}

			descriptor.edges.push_back(edgeDescriptor);
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

	Vertex __parseVertex(const rapidjson::Value& vertexValue, PersistException::Error onError)
	{
		if(!vertexValue.IsObject() || !vertexValue.HasMember("posn")) throw PersistException(onError);

		// Vertex is a plain POD struct with no member initialisers, so it must be zeroed before
		// filling in whichever fields the JSON actually supplies.
		Vertex vertex{};

		__readDoubleArray(vertexValue["posn"], vertex.posn, 3, onError);

		if(vertexValue.HasMember("colour"))
		{
			const rapidjson::Value& colourValue = vertexValue["colour"];

			if(!colourValue.IsArray() || colourValue.Size() != 4) throw PersistException(onError);

			for(unsigned i = 0; i < 4; i++)
			{
				if(!colourValue[i].IsInt()) throw PersistException(onError);

				int component = colourValue[i].GetInt();

				if(component < 0 || component > 255) throw PersistException(onError);

				vertex.colour[i] = static_cast<std::byte>(component);
			}
		}

		if(vertexValue.HasMember("texCoords"))
		{
			__readDoubleArray(vertexValue["texCoords"], vertex.texCoords, 2, onError);
		}

		if(vertexValue.HasMember("normal"))
		{
			__readDoubleArray(vertexValue["normal"], vertex.normal, 3, onError);
		}

		return vertex;
	}

	/**
	 * Read the flat "vertexes" form, where visibility is written against each vertex and the groups it
	 * describes are the runs of consecutive vertexes sharing one.
	 */
	std::vector<HiveVertexGroupDescriptor> __parseVertexes(const rapidjson::Value& vertexesValue)
	{
		std::vector<HiveVertexGroupDescriptor> vertexGroups;

		if(!vertexesValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_VERTEXES);

		for(auto& vertexValue : vertexesValue.GetArray())
		{
			Vertex vertex = __parseVertex(vertexValue, PersistException::JSON_INVALID_VERTEXES);

			// Schema default.
			std::string visibilityName = "ALWAYS";

			if(vertexValue.HasMember("visibility"))
			{
				if(!vertexValue["visibility"].IsString()) throw PersistException(PersistException::JSON_INVALID_VERTEXES);

				// Only checked for being a name here; whether it is a name that exists is HiveBuilder's concern.
				visibilityName = vertexValue["visibility"].GetString();
			}

			// Visibility belongs to a group of vertexes rather than to a vertex, so each run of consecutive
			// vertexes sharing one visibility becomes a single group.
			if(vertexGroups.empty() || vertexGroups.back().visibilityName != visibilityName)
			{
				vertexGroups.push_back(HiveVertexGroupDescriptor{.visibilityName = visibilityName});
			}

			vertexGroups.back().vertexes.push_back(vertex);
		}

		return vertexGroups;
	}

	/**
	 * Read the grouped "vertexGroups" form, where each group states its visibility once for all of the
	 * vertexes it holds.
	 */
	std::vector<HiveVertexGroupDescriptor> __parseVertexGroups(const rapidjson::Value& vertexGroupsValue)
	{
		std::vector<HiveVertexGroupDescriptor> vertexGroups;

		if(!vertexGroupsValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_VERTEX_GROUPS);

		for(auto& vertexGroupValue : vertexGroupsValue.GetArray())
		{
			if(!vertexGroupValue.IsObject() || !vertexGroupValue.HasMember("vertexes") ||
				!vertexGroupValue["vertexes"].IsArray())
			{
				throw PersistException(PersistException::JSON_INVALID_VERTEX_GROUPS);
			}

			HiveVertexGroupDescriptor groupDescriptor;

			// Schema default.
			groupDescriptor.visibilityName = "ALWAYS";

			if(vertexGroupValue.HasMember("visibility"))
			{
				if(!vertexGroupValue["visibility"].IsString())
				{
					throw PersistException(PersistException::JSON_INVALID_VERTEX_GROUPS);
				}

				// Only checked for being a name here; whether it is a name that exists is HiveBuilder's concern.
				groupDescriptor.visibilityName = vertexGroupValue["visibility"].GetString();
			}

			for(auto& vertexValue : vertexGroupValue["vertexes"].GetArray())
			{
				groupDescriptor.vertexes.push_back(__parseVertex(vertexValue, PersistException::JSON_INVALID_VERTEX_GROUPS));
			}

			// A group holding nothing would only ever reach a surface as an empty chunk, so it is left out
			// rather than carried through the build.
			if(!groupDescriptor.vertexes.empty()) vertexGroups.push_back(groupDescriptor);
		}

		return vertexGroups;
	}

	void __parseGeometry(const rapidjson::Value& nodeValue, HiveNodeDescriptor& descriptor)
	{
		bool hasVertexes = nodeValue.HasMember("vertexes");
		bool hasVertexGroups = nodeValue.HasMember("vertexGroups");

		// The two are alternative spellings of one thing, so a node holding both says nothing about the
		// order they would combine in.
		if(hasVertexes && hasVertexGroups)
		{
			throw PersistException(PersistException::JSON_VERTEXES_AND_VERTEX_GROUPS);
		}

		if(hasVertexes) descriptor.vertexGroups = __parseVertexes(nodeValue["vertexes"]);
		else if(hasVertexGroups) descriptor.vertexGroups = __parseVertexGroups(nodeValue["vertexGroups"]);
	}

	void __parseEmitAgentAffectAction(const rapidjson::Value& nodeValue, HiveNodeDescriptor& descriptor)
	{
		if(!nodeValue.HasMember("emitAgentAffectAction")) return;

		if(!nodeValue["emitAgentAffectAction"].IsBool())
		{
			throw PersistException(PersistException::JSON_INVALID_EMIT_AGENT_AFFECT_ACTION);
		}

		descriptor.emitAgentAffectAction = nodeValue["emitAgentAffectAction"].GetBool();
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

	void __parseAgentFields(const rapidjson::Value& nodeValue, HiveNodeDescriptor& descriptor)
	{
		if(!nodeValue.HasMember("capability") || !nodeValue["capability"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_AGENT_CAPABILITY);
		}

		// Only checked for being a name here; whether it is a name that exists is HiveBuilder's concern.
		descriptor.capabilityName = nodeValue["capability"].GetString();

		if(!nodeValue.HasMember("prompts") || !nodeValue["prompts"].IsArray() || nodeValue["prompts"].Empty())
		{
			throw PersistException(PersistException::JSON_INVALID_AGENT_PROMPTS);
		}

		for(auto& promptValue : nodeValue["prompts"].GetArray())
		{
			if(!promptValue.IsObject() ||
				!promptValue.HasMember("nodeType") || !promptValue["nodeType"].IsString() ||
				!promptValue.HasMember("prompt") || !promptValue["prompt"].IsString())
			{
				throw PersistException(PersistException::JSON_INVALID_AGENT_PROMPTS);
			}

			HiveAgentPromptDescriptor promptDescriptor;

			promptDescriptor.nodeTypeName = promptValue["nodeType"].GetString();
			promptDescriptor.prompt = promptValue["prompt"].GetString();

			if(promptValue.HasMember("nodeIdentifier"))
			{
				if(!promptValue["nodeIdentifier"].IsString())
				{
					throw PersistException(PersistException::JSON_INVALID_AGENT_PROMPTS);
				}

				promptDescriptor.nodeIdentifier = promptValue["nodeIdentifier"].GetString();
			}

			if(promptValue.HasMember("terminateOnResponse"))
			{
				if(!promptValue["terminateOnResponse"].IsBool())
				{
					throw PersistException(PersistException::JSON_INVALID_AGENT_PROMPTS);
				}

				promptDescriptor.terminateOnResponse = promptValue["terminateOnResponse"].GetBool();
			}

			descriptor.prompts.push_back(promptDescriptor);
		}

		if(nodeValue.HasMember("autoTriggerAgentAction"))
		{
			if(!nodeValue["autoTriggerAgentAction"].IsBool())
			{
				throw PersistException(PersistException::JSON_INVALID_AGENT_AUTO_TRIGGER);
			}

			descriptor.autoTriggerAgentAction = nodeValue["autoTriggerAgentAction"].GetBool();
		}

		if(nodeValue.HasMember("serialiseEmittedActions"))
		{
			if(!nodeValue["serialiseEmittedActions"].IsBool())
			{
				throw PersistException(PersistException::JSON_INVALID_AGENT_SERIALISE_ACTIONS);
			}

			descriptor.serialiseEmittedActions = nodeValue["serialiseEmittedActions"].GetBool();
		}
	}

	void __parseTriggerFields(const rapidjson::Value& nodeValue, HiveNodeDescriptor& descriptor)
	{
		if(nodeValue.HasMember("emitTriggerOnPoke"))
		{
			if(!nodeValue["emitTriggerOnPoke"].IsBool())
			{
				throw PersistException(PersistException::JSON_INVALID_TRIGGER_EMIT_ON_POKE);
			}

			descriptor.emitTriggerOnPoke = nodeValue["emitTriggerOnPoke"].GetBool();
		}
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

		if(descriptor.type == HiveNodeDescriptor::TELEPORT)
		{
			if(!nodeValue.HasMember("destination")) throw PersistException(PersistException::JSON_INVALID_DESTINATION);

			descriptor.destination = __parseNodeLocation(nodeValue["destination"]);
		}

		if(descriptor.type == HiveNodeDescriptor::SCENE_GEOMETRY ||
			descriptor.type == HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT)
		{
			__parseGeometry(nodeValue, descriptor);

			__parseEmitAgentAffectAction(nodeValue, descriptor);
		}

		if(descriptor.type == HiveNodeDescriptor::SCENE_TRANSFORM ||
			descriptor.type == HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT)
		{
			descriptor.hasTransform = nodeValue.HasMember("transform");

			if(descriptor.hasTransform)
			{
				__readDoubleArray(nodeValue["transform"], descriptor.transform, 16, PersistException::JSON_INVALID_TRANSFORM);
			}

			__parseEmitAgentAffectAction(nodeValue, descriptor);
		}

		if(__isScriptType(descriptor.type))
		{
			__parseScriptSource(nodeValue, descriptor.coreScript, descriptor.pokeScript);
		}

		if(descriptor.type == HiveNodeDescriptor::AGENT)
		{
			__parseAgentFields(nodeValue, descriptor);
		}

		if(descriptor.type == HiveNodeDescriptor::TRIGGER)
		{
			__parseTriggerFields(nodeValue, descriptor);
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
				if(!surfaceValue["focusViewportFraction"].IsNumber())
				{
					throw PersistException(PersistException::JSON_INVALID_SURFACE_FOCUS_VIEWPORT_FRACTION);
				}

				double focusViewportFraction = surfaceValue["focusViewportFraction"].GetDouble();

				// A fraction of the viewport is only meaningful up to the whole of it, which is the bound the
				// schema states, so anything above 1 is rejected here rather than left to set up a camera that
				// cannot frame what it was pointed at.
				if(focusViewportFraction <= 0.0 || focusViewportFraction > 1.0)
				{
					throw PersistException(PersistException::JSON_INVALID_SURFACE_FOCUS_VIEWPORT_FRACTION);
				}

				descriptor.focusViewportFraction = focusViewportFraction;
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
