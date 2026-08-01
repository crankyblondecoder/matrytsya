#include "AnimateScriptNodeToolBindings.hpp"

#include "../agent/AgentException.hpp"
#include "../agent/ModelToolCallParameterValue.hpp"
#include "../agent/ModelToolDefinition.hpp"
#include "../agent/ModelToolDefinitionParameter.hpp"
#include "../graph/nodes/AnimateScriptNode.hpp"

AnimateScriptNodeToolBindings::AnimateScriptNodeToolBindings(Handle<AnimateScriptNode> node, unsigned serial) :
	_node{node}, _serial{serial}
{
	_registerBinding("getAnimating");
	_registerBinding("setAnimating");
}

std::vector<ModelToolDefinition> AnimateScriptNodeToolBindings::getModelToolDefinitions()
{
	std::vector<ModelToolDefinition> definitions;

	definitions.push_back(ModelToolDefinition(
		"getAnimating",
		"Get whether this node is currently in animating mode.",
		{},
		ModelToolDefinitionParameter("animating", "Whether the node is currently in animating mode.",
			ModelToolDefinitionParameter::PrimitiveType::BOOL)));

	definitions.push_back(ModelToolDefinition(
		"setAnimating",
		"Set whether this node is in animating mode.",
		{
			ModelToolDefinitionParameter("animating", "Whether the node should be marked as animating.",
				ModelToolDefinitionParameter::PrimitiveType::BOOL)
		},
		ModelToolDefinitionParameter("animating", "Whether the node is now in animating mode.",
			ModelToolDefinitionParameter::PrimitiveType::BOOL)));

	return definitions;
}

ModelToolCallParameterValue AnimateScriptNodeToolBindings::processBinding(std::string name,
	std::vector<ModelToolCallParameterValue> parameterValues)
{
	if(name == "getAnimating")
	{
		return ModelToolCallParameterValue("animating", __getAnimating());
	}
	else if(name == "setAnimating")
	{
		bool animating = std::get<bool>(_getParameterValue(parameterValues, "animating").getValue());

		__setAnimating(animating);

		return ModelToolCallParameterValue("animating", animating);
	}
	else
	{
		throw AgentException(AgentException::BINDING_NOT_FOUND);
	}
}

bool AnimateScriptNodeToolBindings::__getAnimating()
{
	return _node.getInstance() -> getAnimating();
}

void AnimateScriptNodeToolBindings::__setAnimating(bool animating)
{
	// Emits action because it is considered to be a similar action to poking the node.
	_node.getInstance() -> setAnimating(animating, _serial, true);
}
