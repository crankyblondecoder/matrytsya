#include "AnimateScriptNode.hpp"

#include "../../lua/lua.hpp"
#include "../actions/AnimateAction.hpp"
#include "../graphActionFlagRegister.hpp"

AnimateScriptNode::~AnimateScriptNode()
{
}

AnimateScriptNode::AnimateScriptNode(const std::string& coreScript, const std::string& pokeScript)
	: StrobeScriptNode(coreScript, pokeScript)
{
	_addActionFlag(ANIMATE_GRAPH_ACTION);
}

void AnimateScriptNode::setAnimating(bool flag, unsigned serial)
{
	// This should never trigger an action being emitted because this is usually called from an action.
	__setAnimating(flag, serial, false);
}

void AnimateScriptNode::__setAnimating(bool animating, unsigned serial, bool emitAnimateAction)
{
	bool emitAction = false;

	{ SYNC(_lock)

		if(serial == 0 || serial > _animatingSerial)
		{
			if(_animating != animating)
			{
				_animating = animating;

				emitAction = emitAnimateAction;
			}

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

AnimateActionTarget* AnimateScriptNode::getAnimateActionTarget()
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
