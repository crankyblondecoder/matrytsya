#include "TriggerEmitterToolBindings.hpp"

#include "../agent/AgentException.hpp"
#include "../agent/ModelToolCallParameterValue.hpp"
#include "../agent/ModelToolDefinition.hpp"
#include "../agent/ModelToolDefinitionParameter.hpp"
#include "../graph/GraphNode.hpp"
#include "../graph/actions/TriggerAction.hpp"

TriggerEmitterToolBindings::TriggerEmitterToolBindings(Handle<GraphNode> node) :
	_node{node}
{
	_registerBinding("emitTrigger");
}

std::vector<ModelToolDefinition> TriggerEmitterToolBindings::getModelToolDefinitions()
{
	std::vector<ModelToolDefinition> definitions;

	definitions.push_back(ModelToolDefinition(
		"emitTrigger",
		"Emit a trigger action from this node. The action is never applied to this node itself, so a node "
		"cannot trigger itself.",
		{
			ModelToolDefinitionParameter("nodeName",
				"If supplied, restricts triggering to nodes with this name.",
				ModelToolDefinitionParameter::PrimitiveType::STRING, false),
			ModelToolDefinitionParameter("nodeType",
				"If supplied, restricts triggering to nodes of this type.",
				ModelToolDefinitionParameter::StringChoice{GraphNode::typeNames()}, false)
		},
		ModelToolDefinitionParameter("triggered", "Whether the trigger action was emitted.",
			ModelToolDefinitionParameter::PrimitiveType::BOOL)));

	return definitions;
}

ModelToolCallParameterValue TriggerEmitterToolBindings::processBinding(std::string name,
	std::vector<ModelToolCallParameterValue> parameterValues)
{
	if(name == "emitTrigger")
	{
		ModelToolCallParameterValue* nodeNameValue = __findParameterValue(parameterValues, "nodeName");
		ModelToolCallParameterValue* nodeTypeValue = __findParameterValue(parameterValues, "nodeType");

		std::string nodeName = nodeNameValue ? std::get<std::string>(nodeNameValue -> getValue()) : "";
		std::string nodeType = nodeTypeValue ? std::get<std::string>(nodeTypeValue -> getValue()) : "";

		__emitTrigger(nodeName, nodeType);

		return ModelToolCallParameterValue("triggered", true);
	}
	else
	{
		throw AgentException(AgentException::BINDING_NOT_FOUND);
	}
}

ModelToolCallParameterValue* TriggerEmitterToolBindings::__findParameterValue(
	std::vector<ModelToolCallParameterValue>& parameterValues, std::string parameterName)
{
	for(ModelToolCallParameterValue& parameterValue : parameterValues)
	{
		if(parameterValue.getParameterName() == parameterName)
		{
			return &parameterValue;
		}
	}

	return nullptr;
}

void TriggerEmitterToolBindings::__emitTrigger(std::string nodeName, std::string nodeType)
{
	bool restrictToNodeType = !nodeType.empty();

	// Action will self delete once complete.
	TriggerAction* action = new TriggerAction(_node, nodeName, restrictToNodeType,
		restrictToNodeType ? GraphNode::typeFromName(nodeType) : GraphNode::Type::GRAPH_NODE);

	action -> incrRef();

	_node.getInstance() -> _emitAction(action);

	action -> decrRef();
}
