#include "SceneGeometryNode.hpp"
#include "../actions/AgentAffectAction.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"
#include "SceneGeometry.hpp"

SceneGeometryNode::~SceneGeometryNode()
{
}

SceneGeometryNode::SceneGeometryNode(bool emitAgentAffectAction)
	: GraphNode(), AgentAffectingActionEmitter(emitAgentAffectAction)
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
	_addActionFlag(AGENT_AFFECT_GRAPH_ACTION);
}

GraphNodeType SceneGeometryNode::getType()
{
	return GraphNodeType::SCENE_GEOMETRY_NODE;
}

void SceneGeometryNode::populateSurface(Handle<GraphHiveSceneSurface> surface)
{
	SceneGeometry::populateSurface(surface, getId(), getPokeEnabled());
}

void SceneGeometryNode::strobe()
{
}

void SceneGeometryNode::setStrobe(bool flag)
{
	_strobe = flag;
}

SceneActionTarget* SceneGeometryNode::getSceneActionTarget()
{
	return this;
}

StrobeActionTarget* SceneGeometryNode::getStrobeActionTarget()
{
	return this;
}

AgentAffectActionTarget* SceneGeometryNode::getAgentAffectActionTarget()
{
	return this;
}

void SceneGeometryNode::agentAffectingStart(bool direct)
{
	__setAgentVisible(true);
	_agentAffectingStart(direct);
}

void SceneGeometryNode::agentAffectingEnd(bool direct)
{
	__setAgentVisible(false);
	_agentAffectingEnd(direct);
}

void SceneGeometryNode::__setAgentVisible(bool flag)
{
	SceneGeometry::setAgentVisible(flag);
}

void SceneGeometryNode::_poked(GraphPoke poke)
{
}

void SceneGeometryNode::_emitAgentAffectAction(bool agentAffecting)
{
	Handle<GraphNode> handle(this);

	// Action will self delete once complete.
	AgentAffectAction* action = new AgentAffectAction(handle, agentAffecting);

	action -> incrRef();

	_emitAction(action);

	action -> decrRef();
}

unsigned SceneGeometryNode::getVersion()
{
	// The scene version rather than the vertex version, so that a change to the agent visible flag alone is
	// still enough to make a scene action repopulate the surface.
	return SceneGeometry::getSceneVersion();
}
