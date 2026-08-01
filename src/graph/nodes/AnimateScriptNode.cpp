#include "AnimateScriptNode.hpp"

#include "../actions/AnimateAction.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHive.hpp"
#include "../GraphToolBindingsFactory.hpp"
#include "../../agent/ModelToolBindings.hpp"

#include "../../lua/lua.hpp"

AnimateScriptNode::~AnimateScriptNode()
{
}

AnimateScriptNode::AnimateScriptNode(const std::string& coreScript, const std::string& pokeScript)
	: StrobeScriptNode(coreScript, pokeScript)
{
	_addActionFlag(ANIMATE_GRAPH_ACTION);
	_addActionFlag(AGENT_GRAPH_ACTION);
}

void AnimateScriptNode::setAnimating(bool flag, unsigned serial)
{
	// This should never trigger an action being emitted because this is usually called from an action.
	__setAnimating(flag, serial, false);
}

void AnimateScriptNode::setAnimating(bool flag, unsigned serial, bool emitAnimateAction)
{
	__setAnimating(flag, serial, emitAnimateAction);
}

void AnimateScriptNode::__setAnimating(bool animating, unsigned serial, bool emitAnimateAction)
{
	bool emitAction = false;

	{ SYNC(_lock)

		if(serial == 0 || serial > _animatingSerial)
		{
			_animating = animating;

			// An explicit request re-asserts the mode across the subtree even when this node's own flag is
			// unchanged, because a node below may have cleared itself since it was last set. Emitting only on
			// a change here would leave such a node stuck, as nothing would carry the mode back down to it.
			emitAction = emitAnimateAction;

			if(serial != 0) _animatingSerial = serial;
		}
	}

	if(emitAction) _emitAction(new AnimateAction(Handle<GraphNode>(this), _animating));
}

bool AnimateScriptNode::__getAnimating()
{
	{ SYNC(_lock)

		return _animating;
	}
}

bool AnimateScriptNode::getAnimating()
{
	return __getAnimating();
}

AnimateActionTarget* AnimateScriptNode::getAnimateActionTarget()
{
	return this;
}

std::vector<Handle<ModelToolBindings>> AnimateScriptNode::getModelToolBindings(AgenticHarness::Capability capability,
	unsigned serial)
{
	// Only what the hive's factory holds for this class; no script defined tool bindings are supported yet.
	Handle<GraphHive> hive = getHive();

	if(!hive.isValid()) return std::vector<Handle<ModelToolBindings>>();

	Handle<GraphToolBindingsFactory> factory = hive.getInstance() -> getToolBindingsFactory();

	if(!factory.isValid()) return std::vector<Handle<ModelToolBindings>>();

	return factory.getInstance() -> getAnimateScriptNodeToolBindings(capability, serial,
		Handle<AnimateScriptNode>(this));
}

AgentActionTarget* AnimateScriptNode::getAgentActionTarget()
{
	return this;
}

void AnimateScriptNode::_registerCoreGlobals(lua_State* luaState)
{
	StrobeScriptNode::_registerCoreGlobals(luaState);

	__registerAnimatingBindings(luaState);
}

int AnimateScriptNode::__luaGetAnimating(lua_State* luaState)
{
	AnimateScriptNode* node = static_cast<AnimateScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_pushboolean(luaState, node -> __getAnimating());

	return 1;
}

int AnimateScriptNode::__luaSetAnimating(lua_State* luaState)
{
	AnimateScriptNode* node = static_cast<AnimateScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	bool animating = lua_toboolean(luaState, 1);
	bool emitAnimateAction = lua_isnoneornil(luaState, 2) ? false : lua_toboolean(luaState, 2);

	node -> __setAnimating(animating, 0, emitAnimateAction);

	return 0;
}

void AnimateScriptNode::__registerAnimatingBindings(lua_State* luaState)
{
	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaGetAnimating, 1);
	lua_setglobal(luaState, "getAnimating");

	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaSetAnimating, 1);
	lua_setglobal(luaState, "setAnimating");
}
