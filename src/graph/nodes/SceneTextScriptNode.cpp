#include "SceneTextScriptNode.hpp"

#include "../GraphNodeVisitor.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"
#include "../actions/TextChangeAction.hpp"

#include "../../lua/lua.hpp"

#include <iterator>
#include <string.h>

namespace
{
	const SceneTextScriptNode::TextChangeMode TEXT_CHANGE_MODES[] = {SceneTextScriptNode::TextChangeMode::DIRECT,
		SceneTextScriptNode::TextChangeMode::SCRIPT, SceneTextScriptNode::TextChangeMode::LOCKED};

	/// Name of each of TEXT_CHANGE_MODES, in the same order.
	const char* const TEXT_CHANGE_MODE_NAMES[] = {"DIRECT", "SCRIPT", "LOCKED"};
}

SceneTextScriptNode::~SceneTextScriptNode()
{
}

SceneTextScriptNode::SceneTextScriptNode(const std::string& coreScript, const std::string& pokeScript)
	: AnimateScriptNode(coreScript, pokeScript)
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
	_addActionFlag(TEXT_VISIBLE_GRAPH_ACTION);
	_addActionFlag(TEXT_CHANGE_GRAPH_ACTION);
	_addActionFlag(FOCUS_GRAPH_ACTION);
}

GraphNodeType SceneTextScriptNode::getType()
{
	return GraphNodeType::SCENE_TEXT_SCRIPT_NODE;
}

void SceneTextScriptNode::accept(GraphNodeVisitor& visitor)
{
	visitor.visit(*this);
}

void SceneTextScriptNode::_registerCoreGlobals(lua_State* luaState)
{
	AnimateScriptNode::_registerCoreGlobals(luaState);

	__registerTextBindings(luaState);
}

void SceneTextScriptNode::populateSurface(Handle<GraphHiveSceneSurface> surface)
{
	_populateFocusRequest(surface, getId());

	SceneText::populateSurface(surface, getId());
}

void SceneTextScriptNode::strobe()
{
}

SceneActionTarget* SceneTextScriptNode::getSceneActionTarget()
{
	return this;
}

FocusActionTarget* SceneTextScriptNode::getFocusActionTarget()
{
	return this;
}

TextVisibleActionTarget* SceneTextScriptNode::getTextVisibleActionTarget()
{
	return this;
}

void SceneTextScriptNode::setTextVisible(bool visible)
{
	setVisible(visible);
}

TextChangeActionTarget* SceneTextScriptNode::getTextChangeActionTarget()
{
	return this;
}

void SceneTextScriptNode::requestTextChange(TextChangeOperation operation, const std::string& text)
{
	switch(getTextChangeMode())
	{
		case TextChangeMode::DIRECT:
			editText(operation, text);
			break;

		case TextChangeMode::SCRIPT:
		{
			// The handler makes the change itself, so whether it ran is of no further interest here.
			TextChangeRequest request{operation, text};
			_callCoreHandler("onTextChange", __pushTextChangeRequest, &request);
			break;
		}

		case TextChangeMode::LOCKED:
			break;
	}
}

void SceneTextScriptNode::setTextChangeMode(TextChangeMode mode)
{
	{ SYNC(_textChangeModeLock)

		_textChangeMode = mode;
	}
}

SceneTextScriptNode::TextChangeMode SceneTextScriptNode::getTextChangeMode()
{
	{ SYNC(_textChangeModeLock)

		return _textChangeMode;
	}
}

const char* SceneTextScriptNode::textChangeModeName(TextChangeMode mode)
{
	for(size_t i = 0; i < std::size(TEXT_CHANGE_MODES); i++)
	{
		if(TEXT_CHANGE_MODES[i] == mode) return TEXT_CHANGE_MODE_NAMES[i];
	}

	return TEXT_CHANGE_MODE_NAMES[0];
}

bool SceneTextScriptNode::textChangeModeFromName(const char* name, TextChangeMode& mode)
{
	for(size_t i = 0; i < std::size(TEXT_CHANGE_MODES); i++)
	{
		if(strcmp(TEXT_CHANGE_MODE_NAMES[i], name) != 0) continue;

		mode = TEXT_CHANGE_MODES[i];
		return true;
	}

	return false;
}

int SceneTextScriptNode::__pushTextChangeRequest(lua_State* luaState, void* context)
{
	TextChangeRequest* request = static_cast<TextChangeRequest*>(context);

	lua_createtable(luaState, 0, 2); // [request]

	lua_pushstring(luaState, TextChangeAction::operationName(request -> operation)); // [request, operation]
	lua_setfield(luaState, -2, "operation"); // [request]

	lua_pushlstring(luaState, request -> text.data(), request -> text.size()); // [request, text]
	lua_setfield(luaState, -2, "text"); // [request]

	return 1;
}

int SceneTextScriptNode::__luaSetTextChangeMode(lua_State* luaState)
{
	const char* name = luaL_checkstring(luaState, 1);
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	TextChangeMode mode;

	if(!textChangeModeFromName(name, mode))
	{
		return luaL_error(luaState, "text change mode must be \"DIRECT\", \"SCRIPT\" or \"LOCKED\"");
	}

	node -> setTextChangeMode(mode);

	return 0;
}

int SceneTextScriptNode::__luaGetTextChangeMode(lua_State* luaState)
{
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_pushstring(luaState, textChangeModeName(node -> getTextChangeMode()));

	return 1;
}

unsigned SceneTextScriptNode::getVersion()
{
	// A focus request is counted in so that one is enough on its own to make a scene action repopulate the
	// surface.
	return SceneText::getVersion() + getFocusRequestCount();
}

int SceneTextScriptNode::__luaSetText(lua_State* luaState)
{
	size_t length = 0;
	const char* text = luaL_checklstring(luaState, 1, &length);
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	node -> setText(std::string(text, length));

	return 0;
}

int SceneTextScriptNode::__luaGetText(lua_State* luaState)
{
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	std::string text = node -> getText();

	lua_pushlstring(luaState, text.data(), text.size());

	return 1;
}

int SceneTextScriptNode::__luaSetTextPosition(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TTABLE);

	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	double position[3];

	for(int i = 0; i < 3; i++)
	{
		lua_geti(luaState, 1, i + 1); // [position, element]
		position[i] = luaL_checknumber(luaState, -1);
		lua_pop(luaState, 1); // [position]
	}

	node -> setPosition(position);

	return 0;
}

int SceneTextScriptNode::__luaGetTextPosition(lua_State* luaState)
{
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	double position[3];

	node -> getPosition(position);

	lua_createtable(luaState, 3, 0); // [position]

	for(int i = 0; i < 3; i++)
	{
		lua_pushnumber(luaState, position[i]);
		lua_seti(luaState, -2, i + 1); // [position]
	}

	return 1;
}

int SceneTextScriptNode::__luaSetTextHeight(lua_State* luaState)
{
	lua_Number height = luaL_checknumber(luaState, 1);
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	if(!node -> setHeight(height)) return luaL_error(luaState, "text height must be greater than zero");

	return 0;
}

int SceneTextScriptNode::__luaGetTextHeight(lua_State* luaState)
{
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_pushnumber(luaState, node -> getHeight());

	return 1;
}

int SceneTextScriptNode::__luaSetTextColour(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TTABLE);

	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	std::byte colour[4];

	__luaCheckColour(luaState, 1, "text colour", colour);

	node -> setColour(colour);

	return 0;
}

int SceneTextScriptNode::__luaGetTextColour(lua_State* luaState)
{
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	std::byte colour[4];

	node -> getColour(colour);

	__luaPushColour(luaState, colour);

	return 1;
}

int SceneTextScriptNode::__luaSetTextBackground(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TTABLE);

	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	// Starts from what is there so that a script can change one part of the box without restating the rest.
	TextBackground background = node -> getBackground();

	if(lua_getfield(luaState, 1, "fillColour") != LUA_TNIL) // [background, fillColour]
	{
		__luaCheckColour(luaState, lua_gettop(luaState), "text background fillColour", background.fillColour);
	}

	lua_pop(luaState, 1); // [background]

	if(lua_getfield(luaState, 1, "borderColour") != LUA_TNIL) // [background, borderColour]
	{
		__luaCheckColour(luaState, lua_gettop(luaState), "text background borderColour", background.borderColour);
	}

	lua_pop(luaState, 1); // [background]

	if(lua_getfield(luaState, 1, "borderWidth") != LUA_TNIL) // [background, borderWidth]
	{
		if(!lua_isnumber(luaState, -1)) return luaL_error(luaState, "text background borderWidth must be a number");

		background.borderWidth = lua_tonumber(luaState, -1);
	}

	lua_pop(luaState, 1); // [background]

	if(lua_getfield(luaState, 1, "padding") != LUA_TNIL) // [background, padding]
	{
		if(!lua_isnumber(luaState, -1)) return luaL_error(luaState, "text background padding must be a number");

		background.padding = lua_tonumber(luaState, -1);
	}

	lua_pop(luaState, 1); // [background]

	int radiusType = lua_getfield(luaState, 1, "cornerRadius"); // [background, cornerRadius]

	if(radiusType == LUA_TNUMBER)
	{
		double radius = lua_tonumber(luaState, -1);

		for(int corner = 0; corner < TextBackground::CORNER_COUNT; corner++) background.cornerRadii[corner] = radius;
	}
	else if(radiusType == LUA_TTABLE)
	{
		for(int corner = 0; corner < TextBackground::CORNER_COUNT; corner++)
		{
			lua_geti(luaState, -1, corner + 1); // [background, cornerRadius, radius]

			if(!lua_isnumber(luaState, -1))
			{
				return luaL_error(luaState, "text background cornerRadius must be a number or 4 numbers");
			}

			background.cornerRadii[corner] = lua_tonumber(luaState, -1);
			lua_pop(luaState, 1); // [background, cornerRadius]
		}
	}
	else if(radiusType != LUA_TNIL)
	{
		return luaL_error(luaState, "text background cornerRadius must be a number or 4 numbers");
	}

	lua_pop(luaState, 1); // [background]

	if(!node -> setBackground(background))
	{
		return luaL_error(luaState, "text background lengths must be finite and not negative");
	}

	return 0;
}

int SceneTextScriptNode::__luaGetTextBackground(lua_State* luaState)
{
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	TextBackground background = node -> getBackground();

	lua_createtable(luaState, 0, 5); // [background]

	__luaPushColour(luaState, background.fillColour); // [background, fillColour]
	lua_setfield(luaState, -2, "fillColour"); // [background]

	__luaPushColour(luaState, background.borderColour); // [background, borderColour]
	lua_setfield(luaState, -2, "borderColour"); // [background]

	lua_pushnumber(luaState, background.borderWidth); // [background, borderWidth]
	lua_setfield(luaState, -2, "borderWidth"); // [background]

	lua_pushnumber(luaState, background.padding); // [background, padding]
	lua_setfield(luaState, -2, "padding"); // [background]

	lua_createtable(luaState, TextBackground::CORNER_COUNT, 0); // [background, cornerRadius]

	for(int corner = 0; corner < TextBackground::CORNER_COUNT; corner++)
	{
		lua_pushnumber(luaState, background.cornerRadii[corner]);
		lua_seti(luaState, -2, corner + 1); // [background, cornerRadius]
	}

	lua_setfield(luaState, -2, "cornerRadius"); // [background]

	return 1;
}

void SceneTextScriptNode::__luaCheckColour(lua_State* luaState, int index, const char* name, std::byte colour[4])
{
	if(!lua_istable(luaState, index))
	{
		luaL_error(luaState, "%s must be a table of 4 integers", name);
		return;
	}

	for(int i = 0; i < 4; i++)
	{
		lua_geti(luaState, index, i + 1); // [..., element]

		int isInteger = 0;
		lua_Integer channel = lua_tointegerx(luaState, -1, &isInteger);
		lua_pop(luaState, 1); // [...]

		if(!isInteger)
		{
			luaL_error(luaState, "%s must be a table of 4 integers", name);
			return;
		}

		if(channel < 0 || channel > 255)
		{
			luaL_error(luaState, "%s channel out of range 0-255: %d", name, static_cast<int>(channel));
			return;
		}

		colour[i] = static_cast<std::byte>(channel);
	}
}

void SceneTextScriptNode::__luaPushColour(lua_State* luaState, const std::byte colour[4])
{
	lua_createtable(luaState, 4, 0); // [colour]

	for(int i = 0; i < 4; i++)
	{
		lua_pushinteger(luaState, std::to_integer<lua_Integer>(colour[i]));
		lua_seti(luaState, -2, i + 1); // [colour]
	}
}

int SceneTextScriptNode::__luaSetTextVisible(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TBOOLEAN);

	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	node -> setVisible(lua_toboolean(luaState, 1));

	return 0;
}

int SceneTextScriptNode::__luaGetTextVisible(lua_State* luaState)
{
	SceneTextScriptNode* node = static_cast<SceneTextScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_pushboolean(luaState, node -> getVisible());

	return 1;
}

void SceneTextScriptNode::__registerTextBindings(lua_State* luaState)
{
	struct Binding
	{
		const char* name;
		lua_CFunction function;
	};

	const Binding bindings[] =
	{
		{"setText", __luaSetText},
		{"getText", __luaGetText},
		{"setTextPosition", __luaSetTextPosition},
		{"getTextPosition", __luaGetTextPosition},
		{"setTextHeight", __luaSetTextHeight},
		{"getTextHeight", __luaGetTextHeight},
		{"setTextColour", __luaSetTextColour},
		{"getTextColour", __luaGetTextColour},
		{"setTextBackground", __luaSetTextBackground},
		{"getTextBackground", __luaGetTextBackground},
		{"setTextVisible", __luaSetTextVisible},
		{"getTextVisible", __luaGetTextVisible},
		{"setTextChangeMode", __luaSetTextChangeMode},
		{"getTextChangeMode", __luaGetTextChangeMode}
	};

	for(const Binding& binding : bindings)
	{
		lua_pushlightuserdata(luaState, this);
		lua_pushcclosure(luaState, binding.function, 1);
		lua_setglobal(luaState, binding.name);
	}
}
