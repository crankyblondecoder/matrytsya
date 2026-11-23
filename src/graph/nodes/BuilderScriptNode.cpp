#include "BuilderScriptNode.hpp"

#include "AgentNode.hpp"
#include "PingNode.hpp"
#include "SceneGeometry.hpp"
#include "SceneGeometryNode.hpp"
#include "SceneGeometryScriptNode.hpp"
#include "SceneImageNode.hpp"
#include "SceneRootNode.hpp"
#include "SceneTextScriptNode.hpp"
#include "SceneTransformNode.hpp"
#include "SceneTransformScriptNode.hpp"
#include "ScriptSession.hpp"
#include "StrobeEmitterNode.hpp"
#include "TeleportNode.hpp"
#include "TriggerNode.hpp"
#include "../GraphNodeVisitor.hpp"
#include "../actions/AgentAction.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../graphSceneElements.hpp"
#include "../GraphEdge.hpp"
#include "../GraphException.hpp"
#include "../GraphHive.hpp"
#include "../GraphNamed.hpp"
#include "../GraphNodeLocation.hpp"
#include "../GraphToolBindingsFactory.hpp"
#include "../ImageException.hpp"
#include "../pngImage.hpp"
#include "../TextBackground.hpp"

#include "../../agent/AgenticHarness.hpp"
#include "../../agent/ModelToolBindings.hpp"
#include "../../lua/lua.hpp"
#include "../../thread/thread.hpp"
#include "../../thread/ThreadException.hpp"
#include "../../util/base64.hpp"
#include "../../log/log.hpp"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <utility>

// Note: The vendored Lua is compiled as C, so a Lua error is raised with longjmp, which skips the destructor of
// every C++ object alive in the frames it unwinds. Every binding here that builds something therefore does all of
// its work - reading and checking its arguments included - in a member that never raises, and only reports the
// outcome, raising if it has to, once that member has returned and everything it held is gone. The readers below
// only ever use calls that cannot raise for a reason of the script's making (raw reads, type tests), and report a
// malformed value through an error buffer rather than through luaL_error.

namespace
{
	/// Size of the buffer a binding's reason is written into. Plain storage, so a raised error has nothing to skip
	/// the destructor of.
	const size_t ERROR_SIZE = 256;

	// -- Fields each create binding takes --

	const char* const PING_FIELDS[] = {"name", "pokeEnabled"};
	const char* const TELEPORT_FIELDS[] = {"name", "pokeEnabled", "destination"};
	const char* const SCENE_ROOT_FIELDS[] = {"name", "pokeEnabled", "strobePeriodMs"};
	const char* const SCENE_GEOMETRY_FIELDS[] = {"name", "pokeEnabled", "emitAgentAffectAction", "vertexGroups"};
	const char* const SCENE_GEOMETRY_SCRIPT_FIELDS[] = {"name", "pokeEnabled", "coreScript", "pokeScript",
		"emitAgentAffectAction", "vertexGroups"};
	const char* const SCENE_TRANSFORM_FIELDS[] = {"name", "pokeEnabled", "emitAgentAffectAction", "transform"};
	const char* const SCENE_TRANSFORM_SCRIPT_FIELDS[] = {"name", "pokeEnabled", "coreScript", "pokeScript",
		"emitAgentAffectAction", "transform"};
	const char* const SCENE_TEXT_SCRIPT_FIELDS[] = {"name", "pokeEnabled", "coreScript", "pokeScript", "text",
		"textPosition", "textHeight", "textColour", "textBackground", "textVisible"};
	const char* const AGENT_FIELDS[] = {"name", "pokeEnabled", "capability", "prompts", "autoTriggerAgentAction",
		"serialiseEmittedActions"};
	const char* const TRIGGER_FIELDS[] = {"name", "pokeEnabled", "coreScript", "pokeScript", "emitTriggerOnPoke"};
	const char* const SCENE_IMAGE_FIELDS[] = {"name", "pokeEnabled", "imageData", "imageFile", "imageUrl",
		"imagePosition", "imageHeight", "imageBackground", "imageVisible"};

	// -- Fields of the nested tables those take --

	const char* const DESTINATION_FIELDS[] = {"hostname", "port", "hiveName", "nodeName"};
	const char* const VERTEX_GROUP_FIELDS[] = {"visibility", "vertexes"};
	const char* const VERTEX_FIELDS[] = {"posn", "colour", "texCoords", "normal"};
	const char* const TEXT_BACKGROUND_FIELDS[] = {"fillColour", "borderColour", "borderWidth", "padding",
		"cornerRadius"};
	const char* const PROMPT_FIELDS[] = {"nodeIdentifier", "nodeType", "prompt", "terminateOnResponse"};
	const char* const EDGE_OPTION_FIELDS[] = {"actionFlags", "traversalKey", "actionsCompleteAfterTraverse"};

	/**
	 * Pairs an action flag with the name it is exposed under in the ActionFlag constant table. The names are
	 * those a hive definition's edges name their action flags by.
	 */
	struct ActionFlagName
	{
		/// Name the flag is exposed under.
		const char* name;

		/// The flag's bit.
		unsigned long flag;
	};

	const ActionFlagName ACTION_FLAGS[] =
	{
		{"PING_GRAPH_ACTION", PING_GRAPH_ACTION},
		{"SERIALISABLE_GRAPH_ACTION", SERIALISABLE_GRAPH_ACTION},
		{"SCRIPT_GRAPH_ACTION", SCRIPT_GRAPH_ACTION},
		{"SCENE_GRAPH_ACTION", SCENE_GRAPH_ACTION},
		{"SCENE_STROBE_GRAPH_ACTION", SCENE_STROBE_GRAPH_ACTION},
		{"ANIMATE_GRAPH_ACTION", ANIMATE_GRAPH_ACTION},
		{"AGENT_GRAPH_ACTION", AGENT_GRAPH_ACTION},
		{"TRIGGER_GRAPH_ACTION", TRIGGER_GRAPH_ACTION},
		{"AGENT_AFFECT_GRAPH_ACTION", AGENT_AFFECT_GRAPH_ACTION},
		{"TEXT_VISIBLE_GRAPH_ACTION", TEXT_VISIBLE_GRAPH_ACTION}
	};

	// Every VertexVisibility, paired with the name the VertexVisibility constant table exposes it under. The
	// values are the same ones a scene geometry script node exposes, so a constant means the same in either.
	const SceneGeometry::VertexVisibility VERTEX_VISIBILITIES[] = {
		SceneGeometry::VertexVisibility::ALWAYS,
		SceneGeometry::VertexVisibility::AGENT,
		SceneGeometry::VertexVisibility::GRABBED,
		SceneGeometry::VertexVisibility::DRAGGING,
		SceneGeometry::VertexVisibility::HOVERED_OVER};

	const char* const VERTEX_VISIBILITY_NAMES[] = {"ALWAYS", "AGENT", "GRABBED", "DRAGGING", "HOVERED_OVER"};

	const AgenticHarness::Capability CAPABILITIES[] = {
		AgenticHarness::Capability::LOW,
		AgenticHarness::Capability::MEDIUM,
		AgenticHarness::Capability::HIGH};

	/**
	 * A run of vertexes sharing one visibility, as read from one entry of a vertexGroups field.
	 */
	struct VertexGroup
	{
		/// Vertexes of the group, in the order given.
		std::vector<Vertex> vertexes;

		/// Visibility every vertex of the group has.
		SceneGeometry::VertexVisibility visibility;
	};

	// -- Error reporting --

	/**
	 * Write a reason into an error buffer.
	 * @param error Buffer of ERROR_SIZE bytes to write into.
	 * @param format printf style format of the reason.
	 * @returns Always false, so a reader can fail with return __fail(...).
	 */
	bool __fail(char* error, const char* format, ...)
	{
		va_list arguments;
		va_start(arguments, format);
		vsnprintf(error, ERROR_SIZE, format, arguments);
		va_end(arguments);

		return false;
	}

	/**
	 * Put the place a reason arose in front of the reason already in an error buffer, so a failure deep in a
	 * nested table names where it was.
	 * @param error Buffer of ERROR_SIZE bytes holding the reason.
	 * @param context Where the reason arose, e.g. "vertexGroups[2]".
	 * @returns Always false.
	 */
	bool __failWithin(char* error, const char* context)
	{
		char reason[ERROR_SIZE];

		snprintf(reason, sizeof(reason), "%s", error);

		return __fail(error, "%s: %s", context, reason);
	}

	// -- Reading Lua values without raising --

	/**
	 * Push the raw value of a named field of a table, bypassing any metamethod so that the read cannot raise.
	 * @param luaState Lua state to read from.
	 * @param index Absolute stack index of the table.
	 * @param field Name of the field to read.
	 * @returns The Lua type of the value pushed.
	 */
	int __rawGetField(lua_State* luaState, int index, const char* field)
	{
		lua_pushstring(luaState, field); // [..., field]
		return lua_rawget(luaState, index); // [..., value]
	}

	/**
	 * Check that a table holds no field other than the given ones, so that a misspelt field fails rather than
	 * being silently ignored.
	 * @param luaState Lua state to read from.
	 * @param index Absolute stack index of the table.
	 * @param allowed Names of the fields the table takes.
	 * @param count Number of names in allowed.
	 * @param error Set to the reason on failure.
	 * @returns True if every field is one of those allowed.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __checkFields(lua_State* luaState, int index, const char* const* allowed, size_t count, char* error)
	{
		lua_pushnil(luaState); // [..., nil]

		while(lua_next(luaState, index) != 0) // [..., key, value]
		{
			// The key is only read as a string once it is known to be one: lua_tostring() on a number key would
			// convert it in place and break the traversal.
			if(lua_type(luaState, -2) != LUA_TSTRING) return __fail(error, "fields must be named");

			const char* field = lua_tostring(luaState, -2);

			bool isAllowed = false;

			for(size_t i = 0; i < count && !isAllowed; i++) isAllowed = strcmp(field, allowed[i]) == 0;

			if(!isAllowed) return __fail(error, "unknown field '%s'", field);

			lua_pop(luaState, 1); // [..., key]
		}

		// lua_next() pops the last key itself once the traversal ends. // [...]

		return true;
	}

	/**
	 * Read an optional or required string field.
	 * @returns True if the field was read, or is absent and not required. out is left untouched if absent.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readStringField(lua_State* luaState, int index, const char* field, bool required, std::string& out,
		char* error)
	{
		int type = __rawGetField(luaState, index, field); // [..., value]

		if(type == LUA_TNIL)
		{
			lua_pop(luaState, 1); // [...]

			return required ? __fail(error, "field '%s' is required", field) : true;
		}

		// A number is not taken as its string form: lua_tolstring() would convert the field in place.
		if(type != LUA_TSTRING) return __fail(error, "field '%s' must be a string", field);

		size_t length = 0;
		const char* value = lua_tolstring(luaState, -1, &length);

		out.assign(value, length);

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read an optional boolean field.
	 * @returns True if the field was read or is absent. out is left untouched if absent.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readBoolField(lua_State* luaState, int index, const char* field, bool& out, char* error)
	{
		int type = __rawGetField(luaState, index, field); // [..., value]

		if(type != LUA_TNIL && type != LUA_TBOOLEAN) return __fail(error, "field '%s' must be a boolean", field);

		if(type == LUA_TBOOLEAN) out = lua_toboolean(luaState, -1);

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read an optional number field.
	 * @returns True if the field was read or is absent. out is left untouched if absent.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readNumberField(lua_State* luaState, int index, const char* field, double& out, char* error)
	{
		int type = __rawGetField(luaState, index, field); // [..., value]

		if(type != LUA_TNIL && type != LUA_TNUMBER) return __fail(error, "field '%s' must be a number", field);

		if(type == LUA_TNUMBER) out = lua_tonumber(luaState, -1);

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read an optional integer field. A float holding an exactly integral value is accepted as that integer.
	 * @param present Set to whether the field was there.
	 * @returns True if the field was read or is absent. out is left untouched if absent.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readIntegerField(lua_State* luaState, int index, const char* field, lua_Integer& out, bool& present,
		char* error)
	{
		int type = __rawGetField(luaState, index, field); // [..., value]

		present = type != LUA_TNIL;

		if(present)
		{
			int isInteger = 0;
			lua_Integer value = lua_tointegerx(luaState, -1, &isInteger);

			// lua_tointegerx() also converts a numeric string, which is not taken as a number here.
			if(type != LUA_TNUMBER || !isInteger) return __fail(error, "field '%s' must be an integer", field);

			out = value;
		}

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read the value at the given stack index as an array of exactly count numbers.
	 * @returns True if it is one. out may be partly written if not.
	 * @note Leaves the stack as it found it.
	 */
	bool __readNumbers(lua_State* luaState, int valueIndex, double* out, int count)
	{
		if(!lua_istable(luaState, valueIndex) || lua_rawlen(luaState, valueIndex) != static_cast<lua_Unsigned>(count))
		{
			return false;
		}

		for(int i = 0; i < count; i++)
		{
			bool isNumber = lua_rawgeti(luaState, valueIndex, i + 1) == LUA_TNUMBER; // [..., element]

			if(isNumber) out[i] = lua_tonumber(luaState, -1);

			lua_pop(luaState, 1); // [...]

			if(!isNumber) return false;
		}

		return true;
	}

	/**
	 * Read an optional or required field holding an array of exactly count numbers.
	 * @returns True if the field was read, or is absent and not required. out is left untouched if absent.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readNumbersField(lua_State* luaState, int index, const char* field, bool required, double* out, int count,
		char* error)
	{
		int type = __rawGetField(luaState, index, field); // [..., value]

		if(type == LUA_TNIL)
		{
			lua_pop(luaState, 1); // [...]

			return required ? __fail(error, "field '%s' is required", field) : true;
		}

		if(!__readNumbers(luaState, lua_gettop(luaState), out, count))
		{
			return __fail(error, "field '%s' must be an array of %d numbers", field, count);
		}

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read the value at the given stack index as a colour: an array of four integers, each 0-255.
	 * @returns True if it is one. out may be partly written if not.
	 * @note Leaves the stack as it found it.
	 */
	bool __readColour(lua_State* luaState, int valueIndex, std::byte out[4])
	{
		if(!lua_istable(luaState, valueIndex) || lua_rawlen(luaState, valueIndex) != 4) return false;

		for(int i = 0; i < 4; i++)
		{
			int isInteger = 0;

			bool isNumber = lua_rawgeti(luaState, valueIndex, i + 1) == LUA_TNUMBER; // [..., element]
			lua_Integer channel = lua_tointegerx(luaState, -1, &isInteger);

			lua_pop(luaState, 1); // [...]

			if(!isNumber || !isInteger || channel < 0 || channel > 255) return false;

			out[i] = static_cast<std::byte>(channel);
		}

		return true;
	}

	/**
	 * Read an optional colour field.
	 * @returns True if the field was read or is absent. out is left untouched if absent.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readColourField(lua_State* luaState, int index, const char* field, std::byte out[4], char* error)
	{
		int type = __rawGetField(luaState, index, field); // [..., value]

		if(type != LUA_TNIL && !__readColour(luaState, lua_gettop(luaState), out))
		{
			return __fail(error, "field '%s' must be an array of 4 integers, each 0-255", field);
		}

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Find the node type whose integer value is the one at the given stack index.
	 * @returns True if the value is an integer that a node type has.
	 */
	bool __readNodeType(lua_State* luaState, int valueIndex, GraphNodeType& type)
	{
		int isInteger = 0;
		lua_Integer value = lua_tointegerx(luaState, valueIndex, &isInteger);

		if(lua_type(luaState, valueIndex) != LUA_TNUMBER || !isInteger) return false;

		for(const std::string& name : GraphNode::typeNames())
		{
			GraphNodeType candidate = GraphNode::typeFromName(name);

			if(static_cast<lua_Integer>(candidate) != value) continue;

			type = candidate;
			return true;
		}

		return false;
	}

	/**
	 * Find the action flag whose value is the one at the given stack index.
	 * @returns True if the value is an integer that an ActionFlag constant has.
	 */
	bool __readActionFlag(lua_State* luaState, int valueIndex, unsigned long& flag)
	{
		int isInteger = 0;
		lua_Integer value = lua_tointegerx(luaState, valueIndex, &isInteger);

		if(lua_type(luaState, valueIndex) != LUA_TNUMBER || !isInteger) return false;

		for(const ActionFlagName& candidate : ACTION_FLAGS)
		{
			if(static_cast<lua_Integer>(candidate.flag) != value) continue;

			flag = candidate.flag;
			return true;
		}

		return false;
	}

	// -- Reading the nested tables of a create binding's fields --

	/**
	 * Read the value at the given stack index as one vertex: {posn, [colour], [texCoords], [normal]}.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readVertex(lua_State* luaState, int valueIndex, Vertex& vertex, char* error)
	{
		if(!lua_istable(luaState, valueIndex)) return __fail(error, "must be a table");

		if(!__checkFields(luaState, valueIndex, VERTEX_FIELDS, std::size(VERTEX_FIELDS), error)) return false;

		// Vertex is a plain POD struct with no member initialisers, so it must be zeroed before filling in
		// whichever fields the table actually supplies.
		vertex = Vertex{};

		return __readNumbersField(luaState, valueIndex, "posn", true, vertex.posn, 3, error)
			&& __readColourField(luaState, valueIndex, "colour", vertex.colour, error)
			&& __readNumbersField(luaState, valueIndex, "texCoords", false, vertex.texCoords, 2, error)
			&& __readNumbersField(luaState, valueIndex, "normal", false, vertex.normal, 3, error);
	}

	/**
	 * Read the value at the given stack index as one vertex group: {[visibility], vertexes}.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readVertexGroup(lua_State* luaState, int valueIndex, VertexGroup& group, char* error)
	{
		if(!lua_istable(luaState, valueIndex)) return __fail(error, "must be a table");

		if(!__checkFields(luaState, valueIndex, VERTEX_GROUP_FIELDS, std::size(VERTEX_GROUP_FIELDS), error))
		{
			return false;
		}

		group.visibility = SceneGeometry::VertexVisibility::ALWAYS;

		if(__rawGetField(luaState, valueIndex, "visibility") != LUA_TNIL) // [..., visibility]
		{
			int isInteger = 0;
			lua_Integer value = lua_tointegerx(luaState, -1, &isInteger);

			bool found = false;

			if(lua_type(luaState, -1) == LUA_TNUMBER && isInteger)
			{
				for(SceneGeometry::VertexVisibility candidate : VERTEX_VISIBILITIES)
				{
					if(static_cast<lua_Integer>(candidate) != value) continue;

					group.visibility = candidate;
					found = true;
					break;
				}
			}

			if(!found) return __fail(error, "field 'visibility' must be a VertexVisibility constant");
		}

		lua_pop(luaState, 1); // [...]

		if(__rawGetField(luaState, valueIndex, "vertexes") != LUA_TTABLE) // [..., vertexes]
		{
			return __fail(error, "field 'vertexes' must be an array of vertex tables");
		}

		int vertexesIndex = lua_gettop(luaState);
		lua_Unsigned count = lua_rawlen(luaState, vertexesIndex);

		for(lua_Unsigned i = 1; i <= count; i++)
		{
			lua_rawgeti(luaState, vertexesIndex, static_cast<lua_Integer>(i)); // [..., vertexes, vertex]

			Vertex vertex;

			if(!__readVertex(luaState, lua_gettop(luaState), vertex, error))
			{
				char context[64];
				snprintf(context, sizeof(context), "vertexes[%llu]", static_cast<unsigned long long>(i));

				return __failWithin(error, context);
			}

			group.vertexes.push_back(vertex);

			lua_pop(luaState, 1); // [..., vertexes]
		}

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read the optional vertexGroups field: an array of vertex groups.
	 * @note A group holding no vertexes is left out, as it would only ever reach a surface as an empty chunk.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readVertexGroups(lua_State* luaState, int index, std::vector<VertexGroup>& groups, char* error)
	{
		int type = __rawGetField(luaState, index, "vertexGroups"); // [..., vertexGroups]

		if(type == LUA_TNIL)
		{
			lua_pop(luaState, 1); // [...]
			return true;
		}

		if(type != LUA_TTABLE) return __fail(error, "field 'vertexGroups' must be an array of vertex group tables");

		int groupsIndex = lua_gettop(luaState);
		lua_Unsigned count = lua_rawlen(luaState, groupsIndex);

		for(lua_Unsigned i = 1; i <= count; i++)
		{
			lua_rawgeti(luaState, groupsIndex, static_cast<lua_Integer>(i)); // [..., vertexGroups, group]

			VertexGroup group;

			if(!__readVertexGroup(luaState, lua_gettop(luaState), group, error))
			{
				char context[64];
				snprintf(context, sizeof(context), "vertexGroups[%llu]", static_cast<unsigned long long>(i));

				return __failWithin(error, context);
			}

			if(!group.vertexes.empty()) groups.push_back(std::move(group));

			lua_pop(luaState, 1); // [..., vertexGroups]
		}

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read the required destination field: {hiveName, nodeName, [hostname], [port]}.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readDestination(lua_State* luaState, int index, GraphNodeLocation& destination, char* error)
	{
		if(__rawGetField(luaState, index, "destination") != LUA_TTABLE) // [..., destination]
		{
			return __fail(error, "field 'destination' must be a table");
		}

		int destinationIndex = lua_gettop(luaState);

		std::string hiveName;
		std::string nodeName;
		std::string hostname = "localhost";
		lua_Integer port = 0;
		bool hasPort = false;

		if(!__checkFields(luaState, destinationIndex, DESTINATION_FIELDS, std::size(DESTINATION_FIELDS), error)
			|| !__readStringField(luaState, destinationIndex, "hiveName", true, hiveName, error)
			|| !__readStringField(luaState, destinationIndex, "nodeName", true, nodeName, error)
			|| !__readStringField(luaState, destinationIndex, "hostname", false, hostname, error)
			|| !__readIntegerField(luaState, destinationIndex, "port", port, hasPort, error))
		{
			return __failWithin(error, "destination");
		}

		if(port < 0 || port > INT_MAX) return __fail(error, "destination: field 'port' is out of range");

		destination.setHiveName(hiveName);
		destination.setNodeName(nodeName);
		destination.setHostname(hostname);
		destination.setPort(static_cast<int>(port));

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read an optional background field, such as textBackground, with the same shape setTextBackground() takes.
	 * Fields left out keep the value already in background.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readTextBackground(lua_State* luaState, int index, const char* field, TextBackground& background,
		char* error)
	{
		int type = __rawGetField(luaState, index, field); // [..., background]

		if(type == LUA_TNIL)
		{
			lua_pop(luaState, 1); // [...]
			return true;
		}

		if(type != LUA_TTABLE) return __fail(error, "field '%s' must be a table", field);

		int backgroundIndex = lua_gettop(luaState);

		if(!__checkFields(luaState, backgroundIndex, TEXT_BACKGROUND_FIELDS, std::size(TEXT_BACKGROUND_FIELDS), error)
			|| !__readColourField(luaState, backgroundIndex, "fillColour", background.fillColour, error)
			|| !__readColourField(luaState, backgroundIndex, "borderColour", background.borderColour, error)
			|| !__readNumberField(luaState, backgroundIndex, "borderWidth", background.borderWidth, error)
			|| !__readNumberField(luaState, backgroundIndex, "padding", background.padding, error))
		{
			return __failWithin(error, field);
		}

		int radiusType = __rawGetField(luaState, backgroundIndex, "cornerRadius"); // [..., background, radius]

		if(radiusType == LUA_TNUMBER)
		{
			// A single radius rounds every corner alike.
			double radius = lua_tonumber(luaState, -1);

			for(int corner = 0; corner < TextBackground::CORNER_COUNT; corner++)
				background.cornerRadii[corner] = radius;
		}
		else if(radiusType != LUA_TNIL
			&& !__readNumbers(luaState, lua_gettop(luaState), background.cornerRadii, TextBackground::CORNER_COUNT))
		{
			return __fail(error, "%s: field 'cornerRadius' must be a number or an array of 4 numbers", field);
		}

		lua_pop(luaState, 2); // [...]

		if(!background.isValid()) return __fail(error, "%s: lengths must be finite and not negative", field);

		return true;
	}

	/**
	 * Read the value at the given stack index as one agent prompt: {nodeType, prompt, [nodeIdentifier],
	 * [terminateOnResponse]}.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readPrompt(lua_State* luaState, int valueIndex, AgentAction::NodePrompt& prompt, char* error)
	{
		if(!lua_istable(luaState, valueIndex)) return __fail(error, "must be a table");

		if(!__checkFields(luaState, valueIndex, PROMPT_FIELDS, std::size(PROMPT_FIELDS), error)
			|| !__readStringField(luaState, valueIndex, "prompt", true, prompt.prompt, error)
			|| !__readStringField(luaState, valueIndex, "nodeIdentifier", false, prompt.nodeIdentifier, error)
			|| !__readBoolField(luaState, valueIndex, "terminateOnResponse", prompt.terminateOnResponse, error))
		{
			return false;
		}

		__rawGetField(luaState, valueIndex, "nodeType"); // [..., nodeType]

		if(!__readNodeType(luaState, lua_gettop(luaState), prompt.nodeType))
		{
			return __fail(error, "field 'nodeType' must be a NodeType constant");
		}

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read the required prompts field: a non-empty array of agent prompts.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readPrompts(lua_State* luaState, int index, std::vector<AgentAction::NodePrompt>& prompts, char* error)
	{
		if(__rawGetField(luaState, index, "prompts") != LUA_TTABLE) // [..., prompts]
		{
			return __fail(error, "field 'prompts' must be an array of prompt tables");
		}

		int promptsIndex = lua_gettop(luaState);
		lua_Unsigned count = lua_rawlen(luaState, promptsIndex);

		// An agent node with nothing to send would never do anything.
		if(count == 0) return __fail(error, "field 'prompts' must hold at least one prompt");

		for(lua_Unsigned i = 1; i <= count; i++)
		{
			lua_rawgeti(luaState, promptsIndex, static_cast<lua_Integer>(i)); // [..., prompts, prompt]

			AgentAction::NodePrompt prompt;

			if(!__readPrompt(luaState, lua_gettop(luaState), prompt, error))
			{
				char context[64];
				snprintf(context, sizeof(context), "prompts[%llu]", static_cast<unsigned long long>(i));

				return __failWithin(error, context);
			}

			prompts.push_back(std::move(prompt));

			lua_pop(luaState, 1); // [..., prompts]
		}

		lua_pop(luaState, 1); // [...]

		return true;
	}

	/**
	 * Read the required capability field, by the name the capability is known by.
	 * @note Leaves the stack as it found it, if it succeeds.
	 */
	bool __readCapability(lua_State* luaState, int index, AgenticHarness::Capability& capability, char* error)
	{
		std::string name;

		if(!__readStringField(luaState, index, "capability", true, name, error)) return false;

		for(AgenticHarness::Capability candidate : CAPABILITIES)
		{
			if(AgenticHarness::getCapabilityName(candidate) != name) continue;

			capability = candidate;
			return true;
		}

		return __fail(error, "field 'capability' must be \"LOW\", \"MEDIUM\" or \"HIGH\"");
	}

	/**
	 * Get the fields a create binding takes.
	 * @param count Set to the number of fields.
	 * @returns The fields.
	 */
	const char* const* __fieldsOf(int typeIndex, size_t& count);
}

/**
 * Everything a create binding read out of its fields table.
 */
struct BuilderScriptNode::NodeSpec
{
	/// Name the node is added to the hive under.
	std::string name;

	/// Whether poking is enabled for the node.
	bool pokeEnabled = false;

	/// Core script of a scripted node.
	std::string coreScript;

	/// Poke script of a scripted node.
	std::string pokeScript;

	/// Destination of a teleport node.
	GraphNodeLocation destination;

	/// Period a scene root node is strobed at. Zero to leave it unregistered as a strobe emitter.
	unsigned strobePeriodMs = 0;

	/// Whether a scene geometry or transform node emits agent affect actions.
	bool emitAgentAffectAction = false;

	/// Vertexes of a scene geometry node.
	std::vector<VertexGroup> vertexGroups;

	/// Transform of a scene transform node. Only applied when hasTransform is true.
	Transform transform = {};

	/// Whether transform was supplied.
	bool hasTransform = false;

	/// Text of a scene text node.
	std::string text;

	/// Anchor position of a scene text node's text.
	double textPosition[3] = {0.0, 0.0, 0.0};

	/// Line height of a scene text node's text.
	double textHeight = 1.0;

	/// Colour of a scene text node's text.
	std::byte textColour[4] = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};

	/// Box drawn behind a scene text node's text.
	TextBackground textBackground;

	/// Whether a scene text node's text is shown.
	bool textVisible = true;

	/// Where a scene image node's image comes from.
	ImageSource::Kind imageKind = ImageSource::EMBEDDED;

	/// PNG bytes of a scene image node's embedded image, decoded from its base64.
	std::string imageBytes;

	/// Path of a scene image node's file image or address of its URL image.
	std::string imageReference;

	/// A scene image node's image, once read from imageBytes or imageReference.
	ImageSource imageSource;

	/// Anchor position of a scene image node's image.
	double imagePosition[3] = {0.0, 0.0, 0.0};

	/// Height of a scene image node's image.
	double imageHeight = 1.0;

	/// Box drawn behind a scene image node's image.
	TextBackground imageBackground;

	/// Whether a scene image node's image is shown.
	bool imageVisible = true;

	/// Capability of the model servicing an agent node's prompts.
	AgenticHarness::Capability capability = AgenticHarness::Capability::MEDIUM;

	/// Prompts an agent node emits.
	std::vector<AgentAction::NodePrompt> prompts;

	/// Whether a trigger arriving at an agent node emits an agent action.
	bool autoTriggerAgentAction = true;

	/// Whether actions emitted by an agent node are serialised.
	bool serialiseEmittedActions = true;

	/// Whether poking a trigger node emits a trigger action of its own.
	bool emitTriggerOnPoke = true;
};

namespace
{
	const char* const* __fieldsOf(int typeIndex, size_t& count)
	{
		// Indexed in the order CreatableNodeType declares its members.
		switch(typeIndex)
		{
			case 0: count = std::size(PING_FIELDS); return PING_FIELDS;
			case 1: count = std::size(TELEPORT_FIELDS); return TELEPORT_FIELDS;
			case 2: count = std::size(SCENE_ROOT_FIELDS); return SCENE_ROOT_FIELDS;
			case 3: count = std::size(SCENE_GEOMETRY_FIELDS); return SCENE_GEOMETRY_FIELDS;
			case 4: count = std::size(SCENE_GEOMETRY_SCRIPT_FIELDS); return SCENE_GEOMETRY_SCRIPT_FIELDS;
			case 5: count = std::size(SCENE_TRANSFORM_FIELDS); return SCENE_TRANSFORM_FIELDS;
			case 6: count = std::size(SCENE_TRANSFORM_SCRIPT_FIELDS); return SCENE_TRANSFORM_SCRIPT_FIELDS;
			case 7: count = std::size(SCENE_TEXT_SCRIPT_FIELDS); return SCENE_TEXT_SCRIPT_FIELDS;
			case 8: count = std::size(AGENT_FIELDS); return AGENT_FIELDS;
			case 9: count = std::size(TRIGGER_FIELDS); return TRIGGER_FIELDS;
			default: count = std::size(SCENE_IMAGE_FIELDS); return SCENE_IMAGE_FIELDS;
		}
	}
}

BuilderScriptNode::~BuilderScriptNode()
{
}

BuilderScriptNode::BuilderScriptNode(const std::string& coreScript, const std::string& pokeScript, bool buildOnce,
	bool built)
	: ScriptNode(coreScript, pokeScript), _buildOnce{buildOnce}
{
	if(_buildOnce && built) _buildState = BuildState::BUILT;

	_setEnergyCost(1);

	// Supports agent action.
	_addActionFlag(AGENT_GRAPH_ACTION);

	// Supports being triggered, which runs the core script.
	_addActionFlag(TRIGGER_GRAPH_ACTION);
}

GraphNodeType BuilderScriptNode::getType()
{
	return GraphNodeType::BUILDER_SCRIPT_NODE;
}

void BuilderScriptNode::accept(GraphNodeVisitor& visitor)
{
	visitor.visit(*this);
}

bool BuilderScriptNode::getBuildOnce()
{
	return _buildOnce;
}

bool BuilderScriptNode::getBuilt()
{
	{ SYNC(_buildLock)

		return _buildState == BuildState::BUILT;
	}
}

void BuilderScriptNode::_sessionEnding(bool poke)
{
	bool builtNow = false;

	{ SYNC(_buildLock)

		// Only the session that built uses the build up; one that merely looked the hive up leaves it be.
		if(_buildState == BuildState::BUILDING && _buildingInPoke == poke)
		{
			_buildState = BuildState::BUILT;
			builtNow = true;
		}
	}

	if(builtNow)
	{
		// Whether the build is used up is part of what the hive would have to be rebuilt from.
		Handle<GraphHive> hive = getHive();

		if(hive.isValid()) hive.getInstance() -> nodeStructureChanged();
	}
}

bool BuilderScriptNode::__claimBuild(bool poke)
{
	if(!_buildOnce) return true;

	{ SYNC(_buildLock)

		if(_buildState == BuildState::NOT_BUILT)
		{
			_buildState = BuildState::BUILDING;
			_buildingInPoke = poke;

			return true;
		}

		// Each state runs one session at a time, so the session building on a state is whichever one is running
		// there now.
		return _buildState == BuildState::BUILDING && _buildingInPoke == poke;
	}
}

std::vector<Handle<ModelToolBindings>> BuilderScriptNode::getModelToolBindings(AgenticHarness::Capability capability,
	unsigned serial)
{
	std::vector<Handle<ModelToolBindings>> tools;

	// What the hive's factory holds for a node, which is what every trigger node offers alike.
	Handle<GraphHive> hive = getHive();

	if(hive.isValid())
	{
		Handle<GraphToolBindingsFactory> factory = hive.getInstance() -> getToolBindingsFactory();

		if(factory.isValid())
		{
			tools = factory.getInstance() -> getGraphNodeToolBindings(capability, Handle<GraphNode>(this));
		}
	}

	// On top of those, whatever this node's own core script declared, which is how a model is given the means to
	// build: a script tool that calls the create bindings.
	for(Handle<ModelToolBindings>& scriptTool : _getScriptToolBindings(capability, serial))
	{
		tools.push_back(scriptTool);
	}

	return tools;
}

AgentActionTarget* BuilderScriptNode::getAgentActionTarget()
{
	return this;
}

void BuilderScriptNode::trigger()
{
	try
	{
		{ Handle<ScriptSession> sessionHandle = requestCoreSession();

			sessionHandle.getInstance() -> run();
		}
	}
	catch(ThreadException& ex)
	{
		// Reached from the trigger action's work cycle, which nothing may escape into.
		LOG(Logger::LogLevel::DEBUG, "Trigger dropped; could not obtain a session on the builder's core state.")
	}
}

TriggerActionTarget* BuilderScriptNode::getTriggerActionTarget()
{
	return this;
}

void BuilderScriptNode::_registerCoreGlobals(lua_State* luaState)
{
	ScriptNode::_registerCoreGlobals(luaState);

	__registerBuilderBindings(luaState);
}

void BuilderScriptNode::__registerBuilderBindings(lua_State* luaState)
{
	// Exposed as tables of integer constants that round-trip through __readActionFlag() and __readVertexGroup().
	lua_createtable(luaState, 0, static_cast<int>(std::size(ACTION_FLAGS)));

	for(const ActionFlagName& actionFlag : ACTION_FLAGS)
	{
		lua_pushinteger(luaState, static_cast<lua_Integer>(actionFlag.flag));
		lua_setfield(luaState, -2, actionFlag.name);
	}

	lua_setglobal(luaState, "ActionFlag");

	lua_createtable(luaState, 0, static_cast<int>(std::size(VERTEX_VISIBILITIES)));

	for(size_t i = 0; i < std::size(VERTEX_VISIBILITIES); i++)
	{
		lua_pushinteger(luaState, static_cast<lua_Integer>(VERTEX_VISIBILITIES[i]));
		lua_setfield(luaState, -2, VERTEX_VISIBILITY_NAMES[i]);
	}

	lua_setglobal(luaState, "VertexVisibility");

	// Which state the building bindings are registered into is fixed now, so it is baked in rather than worked
	// out on every call.
	bool poke = _isPokeState(luaState);

	// Every create binding shares one implementation, told which type it creates by its second upvalue.
	for(size_t i = 0; i < std::size(CREATABLE_NODE_TYPES); i++)
	{
		lua_pushlightuserdata(luaState, this);
		lua_pushinteger(luaState, static_cast<lua_Integer>(CREATABLE_NODE_TYPES[i]));
		lua_pushboolean(luaState, poke);
		lua_pushcclosure(luaState, __luaCreateNode, 3);
		lua_setglobal(luaState, CREATE_BINDING_NAMES[i]);
	}

	lua_pushlightuserdata(luaState, this);
	lua_pushboolean(luaState, poke);
	lua_pushcclosure(luaState, __luaCreateEdge, 2);
	lua_setglobal(luaState, "createEdge");

	struct Binding
	{
		const char* name;
		lua_CFunction function;
	};

	const Binding bindings[] =
	{
		{"getHiveName", __luaGetHiveName},
		{"getSelfName", __luaGetSelfName},
		{"getNodeNames", __luaGetNodeNames},
		{"nodeExists", __luaNodeExists},
		{"getNodeInfo", __luaGetNodeInfo},
		{"getNodesOfType", __luaGetNodesOfType}
	};

	for(const Binding& binding : bindings)
	{
		lua_pushlightuserdata(luaState, this);
		lua_pushcclosure(luaState, binding.function, 1);
		lua_setglobal(luaState, binding.name);
	}
}

int BuilderScriptNode::__finishBinding(lua_State* luaState, BuildStatus status, const char* error)
{
	switch(status)
	{
		case BuildStatus::OK:
		{
			lua_pushboolean(luaState, 1);
			return 1;
		}

		case BuildStatus::RUNTIME_ERROR:
		{
			lua_pushboolean(luaState, 0);
			lua_pushstring(luaState, error);
			return 2;
		}

		case BuildStatus::ARG_ERROR:
		{
			break;
		}
	}

	return luaL_error(luaState, "%s", error);
}

// -- Node creation --

int BuilderScriptNode::__luaCreateNode(lua_State* luaState)
{
	BuilderScriptNode* node = static_cast<BuilderScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	CreatableNodeType type = static_cast<CreatableNodeType>(lua_tointeger(luaState, lua_upvalueindex(2)));

	bool poke = lua_toboolean(luaState, lua_upvalueindex(3));

	char error[ERROR_SIZE] = "";

	BuildStatus status = node -> __createNode(luaState, type, poke, error);

	return __finishBinding(luaState, status, error);
}

BuilderScriptNode::BuildStatus BuilderScriptNode::__createNode(lua_State* luaState, CreatableNodeType type,
	bool poke, char* error)
{
	const char* bindingName = CREATE_BINDING_NAMES[static_cast<size_t>(type)];

	try
	{
		if(lua_type(luaState, 1) != LUA_TTABLE)
		{
			__fail(error, "%s: expects a table of fields", bindingName);
			return BuildStatus::ARG_ERROR;
		}

		NodeSpec spec;

		if(!__readNodeSpec(luaState, 1, type, spec, error))
		{
			__failWithin(error, bindingName);
			return BuildStatus::ARG_ERROR;
		}

		// Read before the build is claimed, so that an image that cannot be displayed does not use the build up. It
		// is not an argument error, as a well formed call can still name a file that is missing or not a PNG.
		if(type == CreatableNodeType::SCENE_IMAGE)
		{
			try
			{
				switch(spec.imageKind)
				{
					case ImageSource::EMBEDDED: spec.imageSource = pngImage::fromPng(spec.imageBytes); break;
					case ImageSource::FILE: spec.imageSource = pngImage::fromFile(spec.imageReference); break;
					case ImageSource::URL: spec.imageSource = pngImage::fromUrl(spec.imageReference); break;
				}
			}
			catch(ImageException& exception)
			{
				__fail(error, "%s: %s", bindingName, exception.getMessage());
				return BuildStatus::RUNTIME_ERROR;
			}
		}

		// Claimed only once the arguments are known to be good, so a call that raises never uses the build up.
		if(!__claimBuild(poke))
		{
			__fail(error, "%s: this builder has already built", bindingName);
			return BuildStatus::RUNTIME_ERROR;
		}

		Handle<GraphHive> hive = getHive();

		if(!hive.isValid())
		{
			__fail(error, "%s: this builder is not part of a hive", bindingName);
			return BuildStatus::RUNTIME_ERROR;
		}

		// Checked up front only so that the common case of a taken name costs no construction. Adding the node
		// checks again, under the hive's lock, which is what actually keeps names unique.
		if(hive.getInstance() -> getNode(spec.name).isValid())
		{
			__fail(error, "%s: a node named '%s' already exists", bindingName, spec.name.c_str());
			return BuildStatus::RUNTIME_ERROR;
		}

		GraphNode* created = __constructNode(type, spec);

		// The handle takes over the construction reference, so the node is released on every path out of here,
		// including an exception from one of the calls below.
		Handle<GraphNode> createdHandle(created);
		created -> decrRef();

		created -> setName(spec.name);
		created -> setPokeEnabled(spec.pokeEnabled);

		// The hive takes ownership of a reference of its own, whether or not it accepts the node.
		created -> incrRef();

		if(!hive.getInstance() -> addUniquelyNamedNode(created))
		{
			__fail(error, "%s: '%s' could not be added; the name is taken or the hive is shutting down", bindingName,
				spec.name.c_str());
			return BuildStatus::RUNTIME_ERROR;
		}

		if(type == CreatableNodeType::SCENE_ROOT && spec.strobePeriodMs > 0)
		{
			hive.getInstance() -> setStrobeEmitter(Handle<StrobeEmitterNode>(created -> asStrobeEmitter()),
				spec.strobePeriodMs);
		}

		return BuildStatus::OK;
	}
	catch(...)
	{
		// Nothing may escape back through Lua's C frames.
		__fail(error, "%s: the node could not be constructed", bindingName);
		return BuildStatus::RUNTIME_ERROR;
	}
}

bool BuilderScriptNode::__readNodeSpec(lua_State* luaState, int index, CreatableNodeType type, NodeSpec& spec,
	char* error)
{
	size_t fieldCount = 0;
	const char* const* fields = __fieldsOf(static_cast<int>(type), fieldCount);

	if(!__checkFields(luaState, index, fields, fieldCount, error)
		|| !__readStringField(luaState, index, "name", true, spec.name, error)
		|| !__readBoolField(luaState, index, "pokeEnabled", spec.pokeEnabled, error))
	{
		return false;
	}

	// A longer name would be cut short on being set, and could then collide with a node it was never meant to.
	if(spec.name.empty() || spec.name.size() > GRAPH_HIVE_NAME_MAX_LEN)
	{
		return __fail(error, "field 'name' must be 1 to %d characters", GRAPH_HIVE_NAME_MAX_LEN);
	}

	bool scripted = type == CreatableNodeType::SCENE_GEOMETRY_SCRIPT
		|| type == CreatableNodeType::SCENE_TRANSFORM_SCRIPT || type == CreatableNodeType::SCENE_TEXT_SCRIPT
		|| type == CreatableNodeType::TRIGGER;

	if(scripted && (!__readStringField(luaState, index, "coreScript", false, spec.coreScript, error)
		|| !__readStringField(luaState, index, "pokeScript", false, spec.pokeScript, error)))
	{
		return false;
	}

	switch(type)
	{
		case CreatableNodeType::PING:
		{
			return true;
		}

		case CreatableNodeType::TELEPORT:
		{
			return __readDestination(luaState, index, spec.destination, error);
		}

		case CreatableNodeType::SCENE_ROOT:
		{
			lua_Integer period = 0;
			bool hasPeriod = false;

			if(!__readIntegerField(luaState, index, "strobePeriodMs", period, hasPeriod, error)) return false;

			if(hasPeriod && (period < 1 || period > static_cast<lua_Integer>(UINT_MAX)))
			{
				return __fail(error, "field 'strobePeriodMs' must be a positive integer");
			}

			spec.strobePeriodMs = static_cast<unsigned>(period);

			return true;
		}

		case CreatableNodeType::SCENE_GEOMETRY:
		case CreatableNodeType::SCENE_GEOMETRY_SCRIPT:
		{
			return __readBoolField(luaState, index, "emitAgentAffectAction", spec.emitAgentAffectAction, error)
				&& __readVertexGroups(luaState, index, spec.vertexGroups, error);
		}

		case CreatableNodeType::SCENE_TRANSFORM:
		case CreatableNodeType::SCENE_TRANSFORM_SCRIPT:
		{
			if(!__readBoolField(luaState, index, "emitAgentAffectAction", spec.emitAgentAffectAction, error))
			{
				return false;
			}

			spec.hasTransform = __rawGetField(luaState, index, "transform") != LUA_TNIL; // [..., transform]
			lua_pop(luaState, 1); // [...]

			return __readNumbersField(luaState, index, "transform", false, spec.transform, 16, error);
		}

		case CreatableNodeType::SCENE_TEXT_SCRIPT:
		{
			if(!__readStringField(luaState, index, "text", false, spec.text, error)
				|| !__readNumbersField(luaState, index, "textPosition", false, spec.textPosition, 3, error)
				|| !__readNumberField(luaState, index, "textHeight", spec.textHeight, error)
				|| !__readColourField(luaState, index, "textColour", spec.textColour, error)
				|| !__readTextBackground(luaState, index, "textBackground", spec.textBackground, error)
				|| !__readBoolField(luaState, index, "textVisible", spec.textVisible, error))
			{
				return false;
			}

			// Written so that a NaN fails it too.
			if(!(spec.textHeight > 0.0)) return __fail(error, "field 'textHeight' must be greater than zero");

			return true;
		}

		case CreatableNodeType::AGENT:
		{
			return __readCapability(luaState, index, spec.capability, error)
				&& __readPrompts(luaState, index, spec.prompts, error)
				&& __readBoolField(luaState, index, "autoTriggerAgentAction", spec.autoTriggerAgentAction, error)
				&& __readBoolField(luaState, index, "serialiseEmittedActions", spec.serialiseEmittedActions, error);
		}

		case CreatableNodeType::TRIGGER:
		{
			return __readBoolField(luaState, index, "emitTriggerOnPoke", spec.emitTriggerOnPoke, error);
		}

		case CreatableNodeType::SCENE_IMAGE:
		{
			std::string imageData;
			std::string imageFile;
			std::string imageUrl;

			if(!__readStringField(luaState, index, "imageData", false, imageData, error)
				|| !__readStringField(luaState, index, "imageFile", false, imageFile, error)
				|| !__readStringField(luaState, index, "imageUrl", false, imageUrl, error)
				|| !__readNumbersField(luaState, index, "imagePosition", false, spec.imagePosition, 3, error)
				|| !__readNumberField(luaState, index, "imageHeight", spec.imageHeight, error)
				|| !__readTextBackground(luaState, index, "imageBackground", spec.imageBackground, error)
				|| !__readBoolField(luaState, index, "imageVisible", spec.imageVisible, error))
			{
				return false;
			}

			if(!imageData.empty() + !imageFile.empty() + !imageUrl.empty() != 1)
			{
				return __fail(error, "exactly one of fields 'imageData', 'imageFile' and 'imageUrl' is required");
			}

			if(!imageData.empty())
			{
				spec.imageKind = ImageSource::EMBEDDED;

				if(!base64::decode(imageData, spec.imageBytes))
					return __fail(error, "field 'imageData' must be base64");
			}
			else if(!imageFile.empty())
			{
				spec.imageKind = ImageSource::FILE;
				spec.imageReference = imageFile;
			}
			else
			{
				// Only the scheme is checked: a URL image may be in any format the viewer can display.
				if(!pngImage::isImageUrl(imageUrl))
					return __fail(error, "field 'imageUrl' must be an http or https URL");

				spec.imageKind = ImageSource::URL;
				spec.imageReference = imageUrl;
			}

			// Written so that a NaN fails it too.
			if(!(spec.imageHeight > 0.0)) return __fail(error, "field 'imageHeight' must be greater than zero");

			return true;
		}
	}

	return __fail(error, "unknown node type");
}

GraphNode* BuilderScriptNode::__constructNode(CreatableNodeType type, const NodeSpec& spec)
{
	switch(type)
	{
		case CreatableNodeType::PING:
		{
			return new PingNode();
		}

		case CreatableNodeType::TELEPORT:
		{
			return new TeleportNode(spec.destination);
		}

		case CreatableNodeType::SCENE_ROOT:
		{
			return new SceneRootNode();
		}

		case CreatableNodeType::SCENE_GEOMETRY:
		{
			SceneGeometryNode* node = new SceneGeometryNode(spec.emitAgentAffectAction);

			try
			{
				for(const VertexGroup& group : spec.vertexGroups) node -> addVertexes(group.vertexes, group.visibility);
			}
			catch(...)
			{
				node -> decrRef();
				throw;
			}

			return node;
		}

		case CreatableNodeType::SCENE_GEOMETRY_SCRIPT:
		{
			SceneGeometryScriptNode* node =
				new SceneGeometryScriptNode(spec.coreScript, spec.pokeScript, spec.emitAgentAffectAction);

			try
			{
				for(const VertexGroup& group : spec.vertexGroups) node -> addVertexes(group.vertexes, group.visibility);
			}
			catch(...)
			{
				node -> decrRef();
				throw;
			}

			return node;
		}

		case CreatableNodeType::SCENE_TRANSFORM:
		{
			SceneTransformNode* node = new SceneTransformNode(spec.emitAgentAffectAction);

			if(spec.hasTransform) node -> setTransform(spec.transform);

			return node;
		}

		case CreatableNodeType::SCENE_TRANSFORM_SCRIPT:
		{
			SceneTransformScriptNode* node =
				new SceneTransformScriptNode(spec.coreScript, spec.pokeScript, spec.emitAgentAffectAction);

			if(spec.hasTransform) node -> setTransform(spec.transform);

			return node;
		}

		case CreatableNodeType::SCENE_TEXT_SCRIPT:
		{
			SceneTextScriptNode* node = new SceneTextScriptNode(spec.coreScript, spec.pokeScript);

			try
			{
				node -> setText(spec.text);
				node -> setPosition(spec.textPosition);
				node -> setHeight(spec.textHeight);
				node -> setColour(spec.textColour);
				node -> setBackground(spec.textBackground);
				node -> setVisible(spec.textVisible);
			}
			catch(...)
			{
				node -> decrRef();
				throw;
			}

			return node;
		}

		case CreatableNodeType::AGENT:
		{
			return new AgentNode(spec.capability, spec.prompts, spec.autoTriggerAgentAction,
				spec.serialiseEmittedActions);
		}

		case CreatableNodeType::TRIGGER:
		{
			return new TriggerNode(spec.coreScript, spec.pokeScript, spec.emitTriggerOnPoke);
		}

		case CreatableNodeType::SCENE_IMAGE:
		{
			SceneImageNode* node = new SceneImageNode();

			try
			{
				node -> setSource(spec.imageSource);
				node -> setPosition(spec.imagePosition);
				node -> setHeight(spec.imageHeight);
				node -> setBackground(spec.imageBackground);
				node -> setVisible(spec.imageVisible);
			}
			catch(...)
			{
				node -> decrRef();
				throw;
			}

			return node;
		}
	}

	throw GraphException(GraphException::UNKNOWN);
}

// -- Edge creation --

int BuilderScriptNode::__luaCreateEdge(lua_State* luaState)
{
	BuilderScriptNode* node = static_cast<BuilderScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	bool poke = lua_toboolean(luaState, lua_upvalueindex(2));

	char error[ERROR_SIZE] = "";

	BuildStatus status = node -> __createEdge(luaState, poke, error);

	return __finishBinding(luaState, status, error);
}

BuilderScriptNode::BuildStatus BuilderScriptNode::__createEdge(lua_State* luaState, bool poke, char* error)
{
	try
	{
		if(lua_type(luaState, 1) != LUA_TSTRING || lua_type(luaState, 2) != LUA_TSTRING)
		{
			__fail(error, "createEdge: fromName and toName must be strings");
			return BuildStatus::ARG_ERROR;
		}

		int optionsType = lua_type(luaState, 3);

		if(optionsType != LUA_TNONE && optionsType != LUA_TNIL && optionsType != LUA_TTABLE)
		{
			__fail(error, "createEdge: options must be a table or nil");
			return BuildStatus::ARG_ERROR;
		}

		std::vector<unsigned long> actionFlags;
		lua_Integer traversalKey = 0;
		bool actionsCompleteAfterTraverse = false;

		if(optionsType == LUA_TTABLE)
		{
			bool hasKey = false;

			if(!__checkFields(luaState, 3, EDGE_OPTION_FIELDS, std::size(EDGE_OPTION_FIELDS), error)
				|| !__readIntegerField(luaState, 3, "traversalKey", traversalKey, hasKey, error)
				|| !__readBoolField(luaState, 3, "actionsCompleteAfterTraverse", actionsCompleteAfterTraverse, error))
			{
				__failWithin(error, "createEdge: options");
				return BuildStatus::ARG_ERROR;
			}

			// Key 0 is what an edge with no key restriction holds, so it is accepted as meaning just that.
			if(traversalKey < 0 || traversalKey > static_cast<lua_Integer>(UINT32_MAX))
			{
				__fail(error, "createEdge: options: field 'traversalKey' must be an integer from 0 to 4294967295");
				return BuildStatus::ARG_ERROR;
			}

			int flagsType = __rawGetField(luaState, 3, "actionFlags"); // [..., actionFlags]

			if(flagsType != LUA_TNIL)
			{
				if(flagsType != LUA_TTABLE)
				{
					__fail(error, "createEdge: options: field 'actionFlags' must be an array of ActionFlag constants");
					return BuildStatus::ARG_ERROR;
				}

				int flagsIndex = lua_gettop(luaState);
				lua_Unsigned count = lua_rawlen(luaState, flagsIndex);

				for(lua_Unsigned i = 1; i <= count; i++)
				{
					lua_rawgeti(luaState, flagsIndex, static_cast<lua_Integer>(i)); // [..., actionFlags, flag]

					unsigned long flag = 0;

					if(!__readActionFlag(luaState, lua_gettop(luaState), flag))
					{
						__fail(error, "createEdge: options: actionFlags[%llu] must be an ActionFlag constant",
							static_cast<unsigned long long>(i));
						return BuildStatus::ARG_ERROR;
					}

					actionFlags.push_back(flag);

					lua_pop(luaState, 1); // [..., actionFlags]
				}
			}

			lua_pop(luaState, 1); // [...]
		}

		size_t fromLength = 0;
		size_t toLength = 0;
		const char* fromValue = lua_tolstring(luaState, 1, &fromLength);
		const char* toValue = lua_tolstring(luaState, 2, &toLength);

		std::string fromName(fromValue, fromLength);
		std::string toName(toValue, toLength);

		// Claimed only once the arguments are known to be good, so a call that raises never uses the build up.
		if(!__claimBuild(poke))
		{
			__fail(error, "createEdge: this builder has already built");
			return BuildStatus::RUNTIME_ERROR;
		}

		Handle<GraphHive> hive = getHive();

		if(!hive.isValid())
		{
			__fail(error, "createEdge: this builder is not part of a hive");
			return BuildStatus::RUNTIME_ERROR;
		}

		Handle<GraphNode> fromNode = hive.getInstance() -> getNode(fromName);

		if(!fromNode.isValid())
		{
			__fail(error, "createEdge: no node named '%s'", fromName.c_str());
			return BuildStatus::RUNTIME_ERROR;
		}

		Handle<GraphNode> toNode = hive.getInstance() -> getNode(toName);

		if(!toNode.isValid())
		{
			__fail(error, "createEdge: no node named '%s'", toName.c_str());
			return BuildStatus::RUNTIME_ERROR;
		}

		Handle<GraphEdge> edge = fromNode.getInstance() -> createEdge(toNode, actionFlags,
			static_cast<unsigned>(traversalKey));

		if(!edge.isValid())
		{
			__fail(error, "createEdge: '%s' has no free edge slot, or has left the hive", fromName.c_str());
			return BuildStatus::RUNTIME_ERROR;
		}

		edge.getInstance() -> setActionsCompleteAfterTraverse(actionsCompleteAfterTraverse);

		return BuildStatus::OK;
	}
	catch(...)
	{
		// Nothing may escape back through Lua's C frames.
		__fail(error, "createEdge: the edge could not be created");
		return BuildStatus::RUNTIME_ERROR;
	}
}

// -- Hive lookup --

// Note: Each of these checks its arguments, the only thing about them that can raise for a reason of the script's
// making, before any C++ object comes into existence. What they push afterwards can then only raise for want of
// memory, which every binding in this module already leaves to the state's memory cap to report.

int BuilderScriptNode::__luaGetHiveName(lua_State* luaState)
{
	BuilderScriptNode* node = static_cast<BuilderScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	Handle<GraphHive> hive = node -> getHive();

	if(!hive.isValid())
	{
		lua_pushnil(luaState);
		return 1;
	}

	std::string name = hive.getInstance() -> getName();

	lua_pushlstring(luaState, name.data(), name.size());

	return 1;
}

int BuilderScriptNode::__luaGetSelfName(lua_State* luaState)
{
	BuilderScriptNode* node = static_cast<BuilderScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	std::string name = node -> getName();

	lua_pushlstring(luaState, name.data(), name.size());

	return 1;
}

int BuilderScriptNode::__luaGetNodeNames(lua_State* luaState)
{
	BuilderScriptNode* node = static_cast<BuilderScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	std::vector<std::string> names;

	Handle<GraphHive> hive = node -> getHive();

	if(hive.isValid()) names = hive.getInstance() -> getNodeNames();

	lua_createtable(luaState, static_cast<int>(names.size()), 0); // [names]

	for(size_t i = 0; i < names.size(); i++)
	{
		lua_pushlstring(luaState, names[i].data(), names[i].size());
		lua_rawseti(luaState, -2, static_cast<lua_Integer>(i + 1)); // [names]
	}

	return 1;
}

int BuilderScriptNode::__luaNodeExists(lua_State* luaState)
{
	size_t length = 0;
	const char* nameValue = luaL_checklstring(luaState, 1, &length);

	BuilderScriptNode* node = static_cast<BuilderScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	bool exists = false;

	{
		Handle<GraphHive> hive = node -> getHive();

		if(hive.isValid()) exists = hive.getInstance() -> getNode(std::string(nameValue, length)).isValid();
	}

	lua_pushboolean(luaState, exists);

	return 1;
}

int BuilderScriptNode::__luaGetNodeInfo(lua_State* luaState)
{
	size_t length = 0;
	const char* nameValue = luaL_checklstring(luaState, 1, &length);

	BuilderScriptNode* node = static_cast<BuilderScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	std::string name(nameValue, length);

	// One snapshot answers everything asked here, including the names the node's edges lead to, so the answer is
	// consistent with itself even if the hive changes meanwhile.
	std::vector<GraphHive::NodeCatalogueEntry> catalogue;

	Handle<GraphHive> hive = node -> getHive();

	if(hive.isValid()) catalogue = hive.getInstance() -> catalogueNodes();

	const GraphHive::NodeCatalogueEntry* found = 0;

	for(const GraphHive::NodeCatalogueEntry& entry : catalogue)
	{
		if(entry.name != name) continue;

		found = &entry;
		break;
	}

	if(!found)
	{
		lua_pushnil(luaState);
		return 1;
	}

	lua_createtable(luaState, 0, 4); // [info]

	lua_pushinteger(luaState, static_cast<lua_Integer>(found -> id));
	lua_setfield(luaState, -2, "id"); // [info]

	lua_pushlstring(luaState, found -> name.data(), found -> name.size());
	lua_setfield(luaState, -2, "name"); // [info]

	lua_pushinteger(luaState, static_cast<lua_Integer>(found -> type));
	lua_setfield(luaState, -2, "type"); // [info]

	lua_createtable(luaState, static_cast<int>(found -> edges.size()), 0); // [info, edges]

	for(size_t i = 0; i < found -> edges.size(); i++)
	{
		const GraphHive::EdgeCatalogueEntry& edge = found -> edges[i];

		lua_createtable(luaState, 0, 3); // [info, edges, edge]

		lua_pushinteger(luaState, static_cast<lua_Integer>(edge.toNodeId));
		lua_setfield(luaState, -2, "toNodeId"); // [info, edges, edge]

		// An edge can only lead to a node in the same hive, so the target is in the snapshot unless it left the
		// hive between the edge being read and the snapshot being finished; its name is then left out.
		for(const GraphHive::NodeCatalogueEntry& target : catalogue)
		{
			if(target.id != edge.toNodeId) continue;

			lua_pushlstring(luaState, target.name.data(), target.name.size());
			lua_setfield(luaState, -2, "toNodeName"); // [info, edges, edge]
			break;
		}

		lua_createtable(luaState, 0, 0); // [info, edges, edge, actionFlags]

		lua_Integer flagCount = 0;

		for(const ActionFlagName& actionFlag : ACTION_FLAGS)
		{
			if(!(edge.actionFlags & actionFlag.flag)) continue;

			lua_pushinteger(luaState, static_cast<lua_Integer>(actionFlag.flag));
			lua_rawseti(luaState, -2, ++flagCount); // [info, edges, edge, actionFlags]
		}

		lua_setfield(luaState, -2, "actionFlags"); // [info, edges, edge]

		lua_rawseti(luaState, -2, static_cast<lua_Integer>(i + 1)); // [info, edges]
	}

	lua_setfield(luaState, -2, "edges"); // [info]

	return 1;
}

int BuilderScriptNode::__luaGetNodesOfType(lua_State* luaState)
{
	GraphNodeType type = GraphNodeType::GRAPH_NODE;

	if(!__readNodeType(luaState, 1, type))
	{
		return luaL_error(luaState, "getNodesOfType: nodeType must be a NodeType constant");
	}

	BuilderScriptNode* node = static_cast<BuilderScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	std::vector<GraphHive::NodeCatalogueEntry> catalogue;

	Handle<GraphHive> hive = node -> getHive();

	if(hive.isValid()) catalogue = hive.getInstance() -> catalogueNodes();

	lua_createtable(luaState, 0, 0); // [names]

	lua_Integer count = 0;

	for(const GraphHive::NodeCatalogueEntry& entry : catalogue)
	{
		if(entry.type != type) continue;

		lua_pushlstring(luaState, entry.name.data(), entry.name.size());
		lua_rawseti(luaState, -2, ++count); // [names]
	}

	return 1;
}
