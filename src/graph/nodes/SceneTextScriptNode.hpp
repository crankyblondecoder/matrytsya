#ifndef SCENE_TEXT_SCRIPT_NODE_H
#define SCENE_TEXT_SCRIPT_NODE_H

#include <cstddef>
#include <string>

#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/TextVisibleActionTarget.hpp"
#include "AnimateScriptNode.hpp"
#include "SceneText.hpp"

class GraphHiveSceneSurface;

struct lua_State;

/**
 * Scriptable graph node that puts a block of text into a scene, anchored in the frame of the transforms it is
 * collected under. Its script can read and modify the text and how it is shown via the setText()/getText(),
 * setTextPosition()/getTextPosition(), setTextHeight()/getTextHeight(), setTextColour()/getTextColour(),
 * setTextBackground()/getTextBackground() and setTextVisible()/getTextVisible() Lua globals. Its text can also be shown or hidden by a text visible action
 * reaching it.
 */
class SceneTextScriptNode : public AnimateScriptNode, public SceneActionTarget, public TextVisibleActionTarget,
	public SceneText
{
    public:

		/**
		 * @param coreScript Lua source code that this node runs when invoked.
		 * @param pokeScript Lua source code that this node runs when poked.
		 */
        SceneTextScriptNode(const std::string& coreScript, const std::string& pokeScript);

		GraphNodeType getType() override;

		void populateSurface(Handle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		SceneActionTarget* getSceneActionTarget() override;

		TextVisibleActionTarget* getTextVisibleActionTarget() override;

		void setTextVisible(bool visible) override;

		unsigned getVersion() override;

	protected:

		// Ref counted.
        virtual ~SceneTextScriptNode();

		void _registerCoreGlobals(lua_State* luaState) override;

    private:

        // Do not allow copying.
        SceneTextScriptNode(const SceneTextScriptNode& copyFrom);
        SceneTextScriptNode& operator= (const SceneTextScriptNode& copyFrom);

		/**
		 * Lua-facing `setText(text)`: sets the text of the node bound as this closure's upvalue.
		 * @param luaState Lua state the call is running against; argument 1 is the text string and upvalue 1 is
		 *        a light userdata pointing at the target SceneTextScriptNode.
		 * @returns Always 0.
		 */
		static int __luaSetText(lua_State* luaState);

		/**
		 * Lua-facing `getText()`: returns the text of the node bound as this closure's upvalue.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at the
		 *        target SceneTextScriptNode.
		 * @returns Always 1 (the text is left on the stack).
		 */
		static int __luaGetText(lua_State* luaState);

		/**
		 * Lua-facing `setTextPosition({x, y, z})`: sets where the text of the node bound as this closure's
		 * upvalue is anchored.
		 * @param luaState Lua state the call is running against; argument 1 is a table of three numbers and
		 *        upvalue 1 is a light userdata pointing at the target SceneTextScriptNode.
		 * @returns Always 0.
		 */
		static int __luaSetTextPosition(lua_State* luaState);

		/**
		 * Lua-facing `getTextPosition()`: returns where the text of the node bound as this closure's upvalue is
		 * anchored, as a table of three numbers.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at the
		 *        target SceneTextScriptNode.
		 * @returns Always 1 (the position table is left on the stack).
		 */
		static int __luaGetTextPosition(lua_State* luaState);

		/**
		 * Lua-facing `setTextHeight(height)`: sets the line height of the text of the node bound as this
		 * closure's upvalue.
		 * @param luaState Lua state the call is running against; argument 1 is the height and upvalue 1 is a
		 *        light userdata pointing at the target SceneTextScriptNode.
		 * @returns Always 0.
		 * @throw Raises a Lua error (does not return) if the height is not greater than zero.
		 */
		static int __luaSetTextHeight(lua_State* luaState);

		/**
		 * Lua-facing `getTextHeight()`: returns the line height of the text of the node bound as this closure's
		 * upvalue.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at the
		 *        target SceneTextScriptNode.
		 * @returns Always 1 (the height is left on the stack).
		 */
		static int __luaGetTextHeight(lua_State* luaState);

		/**
		 * Lua-facing `setTextColour({r, g, b, a})`: sets the colour of the text of the node bound as this
		 * closure's upvalue.
		 * @param luaState Lua state the call is running against; argument 1 is a table of four integers, each
		 *        0-255, and upvalue 1 is a light userdata pointing at the target SceneTextScriptNode.
		 * @returns Always 0.
		 * @throw Raises a Lua error (does not return) if a channel is outside 0-255.
		 */
		static int __luaSetTextColour(lua_State* luaState);

		/**
		 * Lua-facing `getTextColour()`: returns the colour of the text of the node bound as this closure's
		 * upvalue, as a table of four integers.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at the
		 *        target SceneTextScriptNode.
		 * @returns Always 1 (the colour table is left on the stack).
		 */
		static int __luaGetTextColour(lua_State* luaState);

		/**
		 * Lua-facing `setTextBackground({fillColour = {r, g, b, a}, borderColour = {r, g, b, a}, borderWidth = w,
		 * padding = p, cornerRadius = r | {topLeft, topRight, bottomRight, bottomLeft}})`: sets the box drawn behind
		 * the text of the node bound as this closure's upvalue. Fields left out of the table keep their current
		 * value.
		 * @param luaState Lua state the call is running against; argument 1 is the background table and upvalue 1
		 *        is a light userdata pointing at the target SceneTextScriptNode.
		 * @returns Always 0.
		 * @throw Raises a Lua error (does not return) if a field is malformed, a colour channel is outside 0-255
		 *        or a length is negative or not finite.
		 */
		static int __luaSetTextBackground(lua_State* luaState);

		/**
		 * Lua-facing `getTextBackground()`: returns the box drawn behind the text of the node bound as this
		 * closure's upvalue, as a table with every field setTextBackground() takes. cornerRadius is always the
		 * four element form.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at the
		 *        target SceneTextScriptNode.
		 * @returns Always 1 (the background table is left on the stack).
		 */
		static int __luaGetTextBackground(lua_State* luaState);

		/**
		 * Read a colour from the table at the given stack index.
		 * @param luaState Lua state holding the table.
		 * @param index Stack index of the table. Must be absolute rather than relative to the top.
		 * @param name What the colour is, for the error raised if it is malformed.
		 * @param colour Set to the colour: R, G, B, A.
		 * @throw Raises a Lua error (does not return) if the value is not a table of four integers, each 0-255.
		 */
		static void __luaCheckColour(lua_State* luaState, int index, const char* name, std::byte colour[4]);

		/**
		 * Push a colour onto the Lua stack as a table of four integers.
		 * @param luaState Lua state to push onto.
		 * @param colour Colour to push: R, G, B, A.
		 */
		static void __luaPushColour(lua_State* luaState, const std::byte colour[4]);

		/**
		 * Lua-facing `setTextVisible(visible)`: shows or hides the text of the node bound as this closure's
		 * upvalue.
		 * @param luaState Lua state the call is running against; argument 1 is a boolean and upvalue 1 is a
		 *        light userdata pointing at the target SceneTextScriptNode.
		 * @returns Always 0.
		 * @throw Raises a Lua error (does not return) if the argument is not a boolean.
		 */
		static int __luaSetTextVisible(lua_State* luaState);

		/**
		 * Lua-facing `getTextVisible()`: returns whether the text of the node bound as this closure's upvalue is
		 * shown.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at the
		 *        target SceneTextScriptNode.
		 * @returns Always 1 (the visibility is left on the stack).
		 */
		static int __luaGetTextVisible(lua_State* luaState);

		/**
		 * Register the text Lua globals against luaState, binding each function to this node instance via
		 * upvalue.
		 * @param luaState Lua state to register the globals against.
		 */
		void __registerTextBindings(lua_State* luaState);
};

#endif
