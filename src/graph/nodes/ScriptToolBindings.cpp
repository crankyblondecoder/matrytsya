#include "ScriptToolBindings.hpp"

#include "ScriptNode.hpp"
#include "../../agent/AgentException.hpp"
#include "../../agent/ModelToolCallParameterValue.hpp"

ScriptToolBindings::~ScriptToolBindings()
{
}

ScriptToolBindings::ScriptToolBindings(Handle<ScriptNode> node, unsigned serial,
	std::vector<ModelToolDefinition> definitions)
	: _node{node}, _serial{serial}, _definitions{definitions}
{
	for(ModelToolDefinition& definition : _definitions) _registerBinding(definition.getName());
}

std::vector<ModelToolDefinition> ScriptToolBindings::getModelToolDefinitions()
{
	return _definitions;
}

ModelToolCallParameterValue ScriptToolBindings::processBinding(std::string name,
	std::vector<ModelToolCallParameterValue> parameterValues)
{
	// The node is held as a handle, so it cannot go away underneath the call, but it can already have been
	// on its way out when these bindings were built.
	if(!_node.isValid()) throw AgentException(AgentException::NODE_NOT_FOUND);

	for(ModelToolDefinition& definition : _definitions)
	{
		if(definition.getName() != name) continue;

		return _node.getInstance() -> __callScriptTool(name, definition, parameterValues, _serial);
	}

	throw AgentException(AgentException::BINDING_NOT_FOUND);
}
