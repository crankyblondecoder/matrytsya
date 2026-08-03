#include "SceneTransformNode.hpp"

#include "../actions/AgentAffectAction.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneTransformNode::~SceneTransformNode()
{
}

SceneTransformNode::SceneTransformNode(bool emitAgentAffectAction)
	: GraphNode(), AgentAffectingActionEmitter(emitAgentAffectAction)
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
	_addActionFlag(AGENT_AFFECT_GRAPH_ACTION);
}

GraphNode::Type SceneTransformNode::getType()
{
	return Type::SCENE_TRANSFORM_NODE;
}

void SceneTransformNode::populateSurface(Handle<GraphHiveSceneSurface> surface)
{
	SceneTransform::populateSurface(surface, getId());
}

void SceneTransformNode::strobe()
{
}

void SceneTransformNode::setStrobe(bool flag)
{
	_strobe = flag;
}

SceneActionTarget* SceneTransformNode::getSceneActionTarget()
{
	return this;
}

StrobeActionTarget* SceneTransformNode::getStrobeActionTarget()
{
	return this;
}

AgentAffectActionTarget* SceneTransformNode::getAgentAffectActionTarget()
{
	return this;
}

void SceneTransformNode::agentAffectingStart(bool direct)
{
	_agentAffectingStart(direct);
}

void SceneTransformNode::agentAffectingEnd(bool direct)
{
	_agentAffectingEnd(direct);
}

void SceneTransformNode::_poked(GraphPoke poke)
{
}

void SceneTransformNode::_emitAgentAffectAction(bool agentAffecting)
{
	Handle<GraphNode> handle(this);

	// Action will self delete once complete.
	AgentAffectAction* action = new AgentAffectAction(handle, agentAffecting);

	action -> incrRef();

	_emitAction(action);

	action -> decrRef();
}

unsigned SceneTransformNode::getVersion()
{
	return GraphVersioned::getVersion();
}
