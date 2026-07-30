#include "HiveBuilder.hpp"

#include "HiveLoader.hpp"
#include "HiveNodeDescriptor.hpp"
#include "HiveSurfaceDescriptor.hpp"
#include "PersistException.hpp"
#include "../graph/GraphEdge.hpp"
#include "../util/Handle.hpp"
#include "../graph/GraphHive.hpp"
#include "../graph/GraphHiveSceneSurface.hpp"
#include "../graph/GraphHiveSurface.hpp"
#include "../graph/GraphNamed.hpp"
#include "../graph/GraphNode.hpp"
#include "../graph/graphActionFlagRegister.hpp"
#include "../graph/actions/AgentAction.hpp"
#include "../graph/nodes/AgentNode.hpp"
#include "../graph/nodes/PingNode.hpp"
#include "../graph/nodes/SceneGeometryNode.hpp"
#include "../graph/nodes/SceneGeometryScriptNode.hpp"
#include "../graph/nodes/SceneRootNode.hpp"
#include "../graph/nodes/SceneTransformNode.hpp"
#include "../graph/nodes/SceneTransformScriptNode.hpp"
#include "../graph/nodes/StrobeEmitterNode.hpp"
#include "../graph/nodes/TeleportNode.hpp"

#include <map>

GraphHive* HiveBuilder::build(HiveLoader& loader, unsigned numThreads)
{
	std::string hiveName = loader.getHiveName();

	if(hiveName.empty() || hiveName.length() > GRAPH_HIVE_NAME_MAX_LEN)
	{
		throw PersistException(PersistException::INVALID_HIVE_NAME);
	}

	unsigned nodeCount = loader.getNodeCount();

	if(nodeCount == 0)
	{
		throw PersistException(PersistException::NO_NODES);
	}

	GraphHive* hive = new GraphHive(numThreads);

	try
	{
		hive -> setName(hiveName);

		std::map<std::string, Handle<GraphNode>> nodesByName;

		// -- Pass 1: create every node, indexed by name --
		for(unsigned i = 0; i < nodeCount; i++)
		{
			HiveNodeDescriptor descriptor = loader.getNode(i);

			if(descriptor.name.empty())
			{
				throw PersistException(PersistException::INVALID_NODE_NAME);
			}

			if(nodesByName.find(descriptor.name) != nodesByName.end())
			{
				throw PersistException(PersistException::DUPLICATE_NODE_NAME);
			}

			GraphNode* node = __createNode(descriptor);

			node -> setName(descriptor.name);
			node -> setPokeEnabled(descriptor.pokeEnabled);

			// Hive manages the node's initial reference count from here.
			hive -> addNode(node);

			nodesByName.emplace(descriptor.name, Handle<GraphNode>(node));
		}

		// -- Pass 2: wire edges (needs every node to already exist by name) --
		for(unsigned i = 0; i < nodeCount; i++)
		{
			HiveNodeDescriptor descriptor = loader.getNode(i);

			Handle<GraphNode>& fromHandle = nodesByName.at(descriptor.name);

			for(HiveEdgeDescriptor& edgeDescriptor : descriptor.edges)
			{
				auto targetIt = nodesByName.find(edgeDescriptor.toNodeName);

				if(targetIt == nodesByName.end())
				{
					throw PersistException(PersistException::EDGE_TARGET_NOT_FOUND);
				}

				std::vector<unsigned long> actionFlags;

				for(std::string& flagName : edgeDescriptor.actionFlagNames)
				{
					actionFlags.push_back(__actionFlagFromName(flagName));
				}

				Handle<GraphEdge> edgeHandle =
					fromHandle.getInstance() -> createEdge(targetIt -> second, actionFlags);

				if(!edgeHandle.isValid())
				{
					throw PersistException(PersistException::EDGE_CREATE_FAILED);
				}
			}
		}

		std::map<std::string, Handle<GraphHiveSurface>> surfacesByName;

		// -- Pass 3: create every surface, indexed by name (needs every node to already exist by name) --
		unsigned surfaceCount = loader.getSurfaceCount();

		for(unsigned i = 0; i < surfaceCount; i++)
		{
			HiveSurfaceDescriptor descriptor = loader.getSurface(i);

			if(descriptor.name.empty())
			{
				throw PersistException(PersistException::INVALID_SURFACE_NAME);
			}

			if(surfacesByName.find(descriptor.name) != surfacesByName.end())
			{
				throw PersistException(PersistException::DUPLICATE_SURFACE_NAME);
			}

			Handle<GraphNode> referencedNode(0);
			bool hasInitialFocusNode = false;
			unsigned initialFocusNodeId = 0;

			if(descriptor.type == HiveSurfaceDescriptor::SCENE_SURFACE)
			{
				auto nodeIt = nodesByName.find(descriptor.sceneRootNodeName);

				if(nodeIt == nodesByName.end())
				{
					throw PersistException(PersistException::SURFACE_NODE_NOT_FOUND);
				}

				referencedNode = nodeIt -> second;

				if(!descriptor.initialFocusNodeName.empty())
				{
					auto focusNodeIt = nodesByName.find(descriptor.initialFocusNodeName);

					if(focusNodeIt == nodesByName.end())
					{
						throw PersistException(PersistException::SURFACE_NODE_NOT_FOUND);
					}

					hasInitialFocusNode = true;
					initialFocusNodeId = focusNodeIt -> second.getInstance() -> getId();
				}
			}

			GraphHiveSurface* surface = __createSurface(descriptor, referencedNode, hasInitialFocusNode, initialFocusNodeId);

			surface -> setName(descriptor.name);
			surface -> setDefault(descriptor.isDefault);

			// Hive manages the surface's initial reference count from here.
			hive -> addSurface(surface);

			surfacesByName.emplace(descriptor.name, Handle<GraphHiveSurface>(surface));
		}

		// -- Pass 4: strobe emitters (needs every node to already exist by name) --
		unsigned strobeEmitterCount = loader.getStrobeEmitterCount();

		for(unsigned i = 0; i < strobeEmitterCount; i++)
		{
			std::string nodeName;
			unsigned periodMs;

			loader.getStrobeEmitter(i, nodeName, periodMs);

			if(periodMs == 0)
			{
				throw PersistException(PersistException::INVALID_STROBE_PERIOD);
			}

			auto targetIt = nodesByName.find(nodeName);

			if(targetIt == nodesByName.end())
			{
				throw PersistException(PersistException::STROBE_EMITTER_NOT_FOUND);
			}

			StrobeEmitterNode* strobeNode = dynamic_cast<StrobeEmitterNode*>(targetIt -> second.getInstance());

			if(!strobeNode)
			{
				throw PersistException(PersistException::STROBE_EMITTER_WRONG_TYPE);
			}

			hive -> setStrobeEmitter(Handle<StrobeEmitterNode>(strobeNode), periodMs);
		}

		// -- Pass 5: strobe surfaces (needs every surface to already exist by name) --
		unsigned strobeSurfaceCount = loader.getStrobeSurfaceCount();

		for(unsigned i = 0; i < strobeSurfaceCount; i++)
		{
			std::string surfaceName;
			unsigned periodMs;

			loader.getStrobeSurface(i, surfaceName, periodMs);

			if(periodMs == 0)
			{
				throw PersistException(PersistException::INVALID_STROBE_PERIOD);
			}

			auto targetIt = surfacesByName.find(surfaceName);

			if(targetIt == surfacesByName.end())
			{
				throw PersistException(PersistException::STROBE_SURFACE_NOT_FOUND);
			}

			hive -> setStrobeSurface(targetIt -> second, periodMs);
		}
	}
	catch(...)
	{
		// Drops the hive's construction reference (nothing else holds one at this point), deleting it.
		hive -> shutdown();
		throw;
	}

	return hive;
}

GraphNode* HiveBuilder::__createNode(const HiveNodeDescriptor& descriptor)
{
	switch(descriptor.type)
	{
		case HiveNodeDescriptor::PING:
		{
			return new PingNode();
		}

		case HiveNodeDescriptor::TELEPORT:
		{
			return new TeleportNode(descriptor.destination);
		}

		case HiveNodeDescriptor::SCENE_ROOT:
		{
			return new SceneRootNode();
		}

		case HiveNodeDescriptor::SCENE_GEOMETRY:
		{
			SceneGeometryNode* node = new SceneGeometryNode();

			if(descriptor.hasVertexes) node -> addVertexes(descriptor.vertexes);

			return node;
		}

		case HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT:
		{
			SceneGeometryScriptNode* node =
				new SceneGeometryScriptNode(descriptor.coreScript, descriptor.pokeScript);

			if(descriptor.hasVertexes) node -> addVertexes(descriptor.vertexes);

			return node;
		}

		case HiveNodeDescriptor::SCENE_TRANSFORM:
		{
			SceneTransformNode* node = new SceneTransformNode();

			if(descriptor.hasTransform) node -> setTransform(descriptor.transform);

			return node;
		}

		case HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT:
		{
			SceneTransformScriptNode* node =
				new SceneTransformScriptNode(descriptor.coreScript, descriptor.pokeScript);

			if(descriptor.hasTransform) node -> setTransform(descriptor.transform);

			return node;
		}

		case HiveNodeDescriptor::AGENT:
		{
			std::vector<AgentAction::NodePrompt> prompts;

			for(const HiveAgentPromptDescriptor& promptDescriptor : descriptor.prompts)
			{
				AgentAction::NodePrompt prompt;

				prompt.nodeIdentifier = promptDescriptor.nodeIdentifier;
				prompt.nodeType = __nodeTypeFromName(promptDescriptor.nodeTypeName);
				prompt.prompt = promptDescriptor.prompt;

				prompts.push_back(prompt);
			}

			return new AgentNode(__capabilityFromName(descriptor.capabilityName), prompts,
				descriptor.autoTriggerAgentAction, descriptor.serialiseEmittedActions);
		}

		default:
		{
			throw PersistException(PersistException::UNKNOWN_NODE_TYPE);
		}
	}
}

GraphHiveSurface* HiveBuilder::__createSurface(const HiveSurfaceDescriptor& descriptor, Handle<GraphNode> referencedNode,
	bool hasInitialFocusNode, unsigned initialFocusNodeId)
{
	switch(descriptor.type)
	{
		case HiveSurfaceDescriptor::SCENE_SURFACE:
		{
			SceneRootNode* sceneRootNode = dynamic_cast<SceneRootNode*>(referencedNode.getInstance());

			if(!sceneRootNode)
			{
				throw PersistException(PersistException::SURFACE_NODE_WRONG_TYPE);
			}

			GraphHiveSceneSurface* sceneSurface = new GraphHiveSceneSurface(Handle<SceneRootNode>(sceneRootNode));

			if(hasInitialFocusNode) sceneSurface -> setInitialFocusNode(initialFocusNodeId, descriptor.focusViewportFraction);

			return sceneSurface;
		}

		default:
		{
			throw PersistException(PersistException::UNKNOWN_SURFACE_TYPE);
		}
	}
}

unsigned long HiveBuilder::__actionFlagFromName(const std::string& name)
{
	if(name == "PING_GRAPH_ACTION") return PING_GRAPH_ACTION;
	if(name == "SERIALISABLE_GRAPH_ACTION") return SERIALISABLE_GRAPH_ACTION;
	if(name == "SCRIPT_GRAPH_ACTION") return SCRIPT_GRAPH_ACTION;
	if(name == "SCENE_GRAPH_ACTION") return SCENE_GRAPH_ACTION;
	if(name == "SCENE_STROBE_GRAPH_ACTION") return SCENE_STROBE_GRAPH_ACTION;
	if(name == "ANIMATE_GRAPH_ACTION") return ANIMATE_GRAPH_ACTION;
	if(name == "AGENT_GRAPH_ACTION") return AGENT_GRAPH_ACTION;
	if(name == "TRIGGER_GRAPH_ACTION") return TRIGGER_GRAPH_ACTION;

	throw PersistException(PersistException::UNKNOWN_ACTION_FLAG);
}

AgenticHarness::Capability HiveBuilder::__capabilityFromName(const std::string& name)
{
	if(name == "LOW") return AgenticHarness::Capability::LOW;
	if(name == "MEDIUM") return AgenticHarness::Capability::MEDIUM;
	if(name == "HIGH") return AgenticHarness::Capability::HIGH;

	throw PersistException(PersistException::UNKNOWN_AGENT_CAPABILITY);
}

GraphNode::Type HiveBuilder::__nodeTypeFromName(const std::string& name)
{
	if(name == "GRAPH_NODE") return GraphNode::Type::GRAPH_NODE;
	if(name == "PING_NODE") return GraphNode::Type::PING_NODE;
	if(name == "SCENE_GEOMETRY_NODE") return GraphNode::Type::SCENE_GEOMETRY_NODE;
	if(name == "SCENE_TRANSFORM_NODE") return GraphNode::Type::SCENE_TRANSFORM_NODE;
	if(name == "SCRIPT_NODE") return GraphNode::Type::SCRIPT_NODE;
	if(name == "SCENE_GEOMETRY_SCRIPT_NODE") return GraphNode::Type::SCENE_GEOMETRY_SCRIPT_NODE;
	if(name == "SCENE_TRANSFORM_SCRIPT_NODE") return GraphNode::Type::SCENE_TRANSFORM_SCRIPT_NODE;
	if(name == "SCENE_ROOT_NODE") return GraphNode::Type::SCENE_ROOT_NODE;
	if(name == "TELEPORT_NODE") return GraphNode::Type::TELEPORT_NODE;
	if(name == "AGENT_NODE") return GraphNode::Type::AGENT_NODE;

	throw PersistException(PersistException::UNKNOWN_AGENT_PROMPT_NODE_TYPE);
}
