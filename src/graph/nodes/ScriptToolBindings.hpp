#ifndef SCRIPT_TOOL_BINDINGS_H
#define SCRIPT_TOOL_BINDINGS_H

#include <string>
#include <vector>

#include "../../agent/ModelToolBindings.hpp"
#include "../../agent/ModelToolDefinition.hpp"
#include "../../util/Handle.hpp"

class ModelToolCallParameterValue;
class ScriptNode;

/**
 * Tool bindings that expose the tools one ScriptNode's core script declared through its getToolCallBindings()
 * global, so a hive author can offer a model tools of their own by writing Lua rather than C++.
 * @note These belong to a single node instance rather than to a node type: two nodes of the same type running
 *       different scripts offer different tools, which is what the rest of the tool machinery calls a node
 *       level tool. They are built fresh for each agent action and withdrawn as it moves on.
 * @note The definitions are read from the script once, when this is constructed, and served from here
 *       afterwards. They are asked for repeatedly over the life of a model request, and each read would
 *       otherwise claim the node's core state.
 */
class ScriptToolBindings : public ModelToolBindings
{
	public:

		/**
		 * Create the script tool bindings.
		 * @param node Node whose core script implements the tools.
		 * @param serial Serial number of the action driving the request, staged for the script as
		 *        TOOL_CALL_SERIAL ahead of each call these bindings make.
		 * @param definitions Tools the node's script declared, already read off its core state.
		 */
		ScriptToolBindings(Handle<ScriptNode> node, unsigned serial,
			std::vector<ModelToolDefinition> definitions);

		virtual std::vector<ModelToolDefinition> getModelToolDefinitions() override;

		virtual ModelToolCallParameterValue processBinding(std::string name,
			std::vector<ModelToolCallParameterValue> parameterValues) override;

	protected:

		// Defined out of line, unlike the other bindings classes: this header is included by ScriptNode.cpp,
		// where an inlinable destructor is speculatively inlined into decrRef() at call sites whose object is
		// a smaller RefCounted, which g++ then reports as an out of bounds access on the object it is not.
		virtual ~ScriptToolBindings();

	private:

		// Disable copying.
		ScriptToolBindings(const ScriptToolBindings& copyFrom);
		ScriptToolBindings& operator= (const ScriptToolBindings& copyFrom);

		/// Node whose core script implements the tools.
		Handle<ScriptNode> _node;

		/// Serial number staged for the script as TOOL_CALL_SERIAL ahead of each call these bindings make.
		unsigned _serial;

		/// Tools the node's script declared, as read off its core state when this was constructed.
		std::vector<ModelToolDefinition> _definitions;
};

#endif
