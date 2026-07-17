#include "HiveBuilder.hpp"

#include "HiveLoader.hpp"
#include "HiveNodeDescriptor.hpp"
#include "PersistException.hpp"
#include "../graph/GraphEdge.hpp"
#include "../graph/GraphHandle.hpp"
#include "../graph/GraphHive.hpp"
#include "../graph/GraphNamed.hpp"
#include "../graph/GraphNode.hpp"
#include "../graph/graphActionFlagRegister.hpp"
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

		std::map<std::string, GraphHandle<GraphNode>> nodesByName;

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

			nodesByName.emplace(descriptor.name, GraphHandle<GraphNode>(node));
		}

		// -- Pass 2: wire edges (needs every node to already exist by name) --
		for(unsigned i = 0; i < nodeCount; i++)
		{
			HiveNodeDescriptor descriptor = loader.getNode(i);

			GraphHandle<GraphNode>& fromHandle = nodesByName.at(descriptor.name);

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

				GraphHandle<GraphEdge> edgeHandle =
					fromHandle.getInstance() -> createEdge(targetIt -> second, actionFlags);

				if(!edgeHandle.isValid())
				{
					throw PersistException(PersistException::EDGE_CREATE_FAILED);
				}
			}
		}

		// -- Pass 3: strobe emitters (needs every node to already exist by name) --
		unsigned strobeEmitterCount = loader.getStrobeEmitterCount();

		for(unsigned i = 0; i < strobeEmitterCount; i++)
		{
			std::string nodeName;
			unsigned frequencyHz;

			loader.getStrobeEmitter(i, nodeName, frequencyHz);

			if(frequencyHz == 0)
			{
				throw PersistException(PersistException::INVALID_STROBE_FREQUENCY);
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

			hive -> setStrobeEmitter(GraphHandle<StrobeEmitterNode>(strobeNode), frequencyHz);
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

		default:
		{
			throw PersistException(PersistException::UNKNOWN_NODE_TYPE);
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

	throw PersistException(PersistException::UNKNOWN_ACTION_FLAG);
}
