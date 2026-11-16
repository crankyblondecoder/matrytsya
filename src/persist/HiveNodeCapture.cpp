#include "HiveNodeCapture.hpp"

#include "PersistException.hpp"
#include "hiveNames.hpp"
#include "../agent/AgenticHarness.hpp"
#include "../graph/GraphEdge.hpp"
#include "../graph/GraphNode.hpp"
#include "../graph/actions/AgentAction.hpp"
#include "../graph/nodes/AgentNode.hpp"
#include "../graph/nodes/BuilderScriptNode.hpp"
#include "../graph/nodes/PingNode.hpp"
#include "../graph/nodes/SceneGeometryNode.hpp"
#include "../graph/nodes/SceneGeometryScriptNode.hpp"
#include "../graph/nodes/SceneRootNode.hpp"
#include "../graph/nodes/SceneTextScriptNode.hpp"
#include "../graph/nodes/SceneTransformNode.hpp"
#include "../graph/nodes/SceneTransformScriptNode.hpp"
#include "../graph/nodes/ScriptNode.hpp"
#include "../graph/nodes/TeleportNode.hpp"
#include "../graph/nodes/TriggerNode.hpp"
#include "../util/Handle.hpp"

HiveNodeCapture::HiveNodeCapture()
{
}

HiveNodeCapture::~HiveNodeCapture()
{
}

HiveNodeDescriptor HiveNodeCapture::capture(GraphNode& node)
{
	HiveNodeCapture visitor;

	node.accept(visitor);

	visitor._descriptor.name = node.getName();
	visitor._descriptor.pokeEnabled = node.getPokeEnabled();
	visitor._descriptor.edges = captureEdges(node);

	return visitor._descriptor;
}

std::vector<HiveEdgeDescriptor> HiveNodeCapture::captureEdges(GraphNode& node)
{
	std::vector<HiveEdgeDescriptor> edges;

	for(Handle<GraphEdge> edge : node.getEdges())
	{
		Handle<GraphNode> toNode = edge.getInstance() -> traverse();

		// An edge whose target has gone could not be rebuilt, as there would be nothing to point it at.
		if(!toNode.isValid()) continue;

		HiveEdgeDescriptor edgeDescriptor;

		edgeDescriptor.toNodeName = toNode.getInstance() -> getName();
		edgeDescriptor.actionFlagNames = hiveNames::actionFlagNames(edge.getInstance() -> getActionFlags());
		edgeDescriptor.actionsCompleteAfterTraverse = edge.getInstance() -> getActionsCompleteAfterTraverse();
		edgeDescriptor.traversalKey = edge.getInstance() -> getTraversalKey();

		edges.push_back(edgeDescriptor);
	}

	return edges;
}

void HiveNodeCapture::visit(GraphNode& node)
{
	throw PersistException(PersistException::HIVE_NOT_CAPTURABLE,
		"'" + node.getName() + "' is a " + GraphNode::typeName(node.getType()));
}

void HiveNodeCapture::visit(PingNode& node)
{
	_descriptor.type = HiveNodeDescriptor::PING;
}

void HiveNodeCapture::visit(TeleportNode& node)
{
	_descriptor.type = HiveNodeDescriptor::TELEPORT;
	_descriptor.destination = node.getDestination();
}

void HiveNodeCapture::visit(SceneRootNode& node)
{
	_descriptor.type = HiveNodeDescriptor::SCENE_ROOT;
}

void HiveNodeCapture::visit(SceneGeometryNode& node)
{
	_descriptor.type = HiveNodeDescriptor::SCENE_GEOMETRY;
	_descriptor.emitAgentAffectAction = node.getEmitAgentAffectAction();

	__captureGeometry(node);
}

void HiveNodeCapture::visit(SceneGeometryScriptNode& node)
{
	_descriptor.type = HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT;
	_descriptor.emitAgentAffectAction = node.getEmitAgentAffectAction();

	__captureGeometry(node);
	__captureScripts(node);
}

void HiveNodeCapture::visit(SceneTransformNode& node)
{
	_descriptor.type = HiveNodeDescriptor::SCENE_TRANSFORM;
	_descriptor.emitAgentAffectAction = node.getEmitAgentAffectAction();

	// A node always holds a transform, identity if it was given none, so writing it out loses nothing.
	_descriptor.hasTransform = true;
	node.getTransform(_descriptor.transform);
}

void HiveNodeCapture::visit(SceneTransformScriptNode& node)
{
	_descriptor.type = HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT;
	_descriptor.emitAgentAffectAction = node.getEmitAgentAffectAction();

	_descriptor.hasTransform = true;
	node.getTransform(_descriptor.transform);

	__captureScripts(node);
}

void HiveNodeCapture::visit(SceneTextScriptNode& node)
{
	_descriptor.type = HiveNodeDescriptor::SCENE_TEXT_SCRIPT;

	_descriptor.text = node.getText();
	node.getPosition(_descriptor.textPosition);
	_descriptor.textHeight = node.getHeight();
	node.getColour(_descriptor.textColour);
	_descriptor.textBackground = node.getBackground();
	_descriptor.textVisible = node.getVisible();

	__captureScripts(node);
}

void HiveNodeCapture::visit(AgentNode& node)
{
	_descriptor.type = HiveNodeDescriptor::AGENT;

	_descriptor.capabilityName = AgenticHarness::getCapabilityName(node.getCapability());

	for(const AgentAction::NodePrompt& prompt : node.getPrompts())
	{
		HiveAgentPromptDescriptor promptDescriptor;

		promptDescriptor.nodeIdentifier = prompt.nodeIdentifier;
		promptDescriptor.nodeTypeName = GraphNode::typeName(prompt.nodeType);
		promptDescriptor.prompt = prompt.prompt;
		promptDescriptor.terminateOnResponse = prompt.terminateOnResponse;

		_descriptor.prompts.push_back(promptDescriptor);
	}

	_descriptor.autoTriggerAgentAction = node.getAutoTriggerAgentAction();
	_descriptor.serialiseEmittedActions = node.getSerialiseEmittedActions();
}

void HiveNodeCapture::visit(TriggerNode& node)
{
	_descriptor.type = HiveNodeDescriptor::TRIGGER;
	_descriptor.emitTriggerOnPoke = node.getEmitTriggerOnPoke();

	__captureScripts(node);
}

void HiveNodeCapture::visit(BuilderScriptNode& node)
{
	_descriptor.type = HiveNodeDescriptor::BUILDER_SCRIPT;
	_descriptor.buildOnce = node.getBuildOnce();
	_descriptor.built = node.getBuilt();

	__captureScripts(node);
}

void HiveNodeCapture::__captureGeometry(const SceneGeometry& geometry)
{
	for(const SceneGeometry::VertexGroup& group : geometry.getVertexGroups())
	{
		HiveVertexGroupDescriptor groupDescriptor;

		groupDescriptor.vertexes = group.vertexes;
		groupDescriptor.visibilityName = hiveNames::vertexVisibilityName(group.visibility);

		_descriptor.vertexGroups.push_back(groupDescriptor);
	}
}

void HiveNodeCapture::__captureScripts(ScriptNode& node)
{
	_descriptor.coreScript = node.getCoreScript();
	_descriptor.pokeScript = node.getPokeScript();
}
