#ifndef SCENE_TEXT_SCRIPT_NODE_H
#define SCENE_TEXT_SCRIPT_NODE_H

#include <cstddef>
#include <string>

#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/TextChangeActionTarget.hpp"
#include "../actionTargets/TextVisibleActionTarget.hpp"
#include "../GraphFocusable.hpp"
#include "AnimateScriptNode.hpp"
#include "SceneText.hpp"
#include "../../thread/thread.hpp"

class GraphHiveSceneSurface;

struct lua_State;

/**
 * Scriptable graph node that puts a block of text into a scene, anchored in the frame of the transforms it is
 * collected under. Its script can read and modify the text and how it is shown via the setText()/getText(),
 * setTextPosition()/getTextPosition(), setTextHeight()/getTextHeight(), setTextColour()/getTextColour(),
 * setTextBackground()/getTextBackground(), setTextVisible()/getTextVisible() and
 * setTextChangeMode()/getTextChangeMode() Lua globals. Its text can also be shown or hidden by a text visible
 * action reaching it, and a focus action reaching it asks the views of the scene to bring it into focus.
 * @note A text change action reaching it is handled according to its text change mode. DIRECT applies the change
 *       at once. SCRIPT calls the core script's onTextChange(request) global, if it defines one, with a table
 *       holding the change's operation ("REPLACE", "APPEND" or "PREPEND") and text; the handler makes whatever
 *       change it decides on itself, through setText(), and what it returns is ignored. LOCKED ignores it.
 */
class SceneTextScriptNode : public AnimateScriptNode, public SceneActionTarget, public TextVisibleActionTarget,
	public TextChangeActionTarget, public GraphFocusable, public SceneText
{
    public:

		/// How a text change action reaching this node is handled.
		enum class TextChangeMode
		{
			/// The change is applied at once.
			DIRECT,
			/// The change is handed to the core script's onTextChange(request) handler.
			SCRIPT,
			/// The change is ignored.
			LOCKED
		};

		/**
		 * Get the name a text change mode is known by to scripts and hive definitions.
		 * @param mode Mode to name.
		 * @returns "DIRECT", "SCRIPT" or "LOCKED".
		 */
		static const char* textChangeModeName(TextChangeMode mode);

		/**
		 * Find the text change mode a name stands for.
		 * @param name "DIRECT", "SCRIPT" or "LOCKED".
		 * @param mode Set to the mode, if the name is one of those.
		 * @returns False, leaving mode unchanged, if the name is not one of those.
		 */
		static bool textChangeModeFromName(const char* name, TextChangeMode& mode);

		/**
		 * @param coreScript Lua source code that this node runs when invoked.
		 * @param pokeScript Lua source code that this node runs when poked.
		 */
        SceneTextScriptNode(const std::string& coreScript, const std::string& pokeScript);

		GraphNodeType getType() override;

		virtual void accept(GraphNodeVisitor& visitor) override;

		void populateSurface(Handle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		SceneActionTarget* getSceneActionTarget() override;

		TextVisibleActionTarget* getTextVisibleActionTarget() override;

		FocusActionTarget* getFocusActionTarget() override;

		void setTextVisible(bool visible) override;

		TextChangeActionTarget* getTextChangeActionTarget() override;

		void requestTextChange(TextChangeOperation operation, const std::string& text) override;

		/**
		 * Set how a text change action reaching this node is handled.
		 * @param mode Mode to handle text changes in.
		 */
		void setTextChangeMode(TextChangeMode mode);

		/**
		 * @returns How a text change action reaching this node is handled.
		 */
		TextChangeMode getTextChangeMode();

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
		 * Lua-facing `setTextChangeMode(mode)`: sets how a text change action reaching the node bound as this
		 * closure's upvalue is handled.
		 * @param luaState Lua state the call is running against; argument 1 is "DIRECT", "SCRIPT" or "LOCKED" and
		 *        upvalue 1 is a light userdata pointing at the target SceneTextScriptNode.
		 * @returns Always 0.
		 * @throw Raises a Lua error (does not return) if the argument is not one of those names.
		 */
		static int __luaSetTextChangeMode(lua_State* luaState);

		/**
		 * Lua-facing `getTextChangeMode()`: returns how a text change action reaching the node bound as this
		 * closure's upvalue is handled, as "DIRECT", "SCRIPT" or "LOCKED".
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at the
		 *        target SceneTextScriptNode.
		 * @returns Always 1 (the mode name is left on the stack).
		 */
		static int __luaGetTextChangeMode(lua_State* luaState);

		/**
		 * Push the request table handed to onTextChange(): {operation = ..., text = ...}.
		 * @param luaState Lua state to push onto.
		 * @param context Points at the TextChangeRequest to push.
		 * @returns Always 1, the number of values pushed.
		 */
		static int __pushTextChangeRequest(lua_State* luaState, void* context);

		/// A text change waiting to be handed to the core script.
		struct TextChangeRequest
		{
			TextChangeOperation operation;
			const std::string& text;
		};

		/**
		 * Register the text Lua globals against luaState, binding each function to this node instance via
		 * upvalue.
		 * @param luaState Lua state to register the globals against.
		 */
		void __registerTextBindings(lua_State* luaState);

		/// Guards _textChangeMode, as text change actions can reach this node while a script is setting it.
		ThreadMutex _textChangeModeLock;

		/// How a text change action reaching this node is handled.
		TextChangeMode _textChangeMode = TextChangeMode::DIRECT;
};

#endif
