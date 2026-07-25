#include "BasicHiveToolBindings.hpp"

#include "../AgentException.hpp"
#include "../ModelToolCallParameterValue.hpp"
#include "../ModelToolDefinition.hpp"
#include "../ModelToolDefinitionParameter.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphNode.hpp"

BasicHiveToolBindings::BasicHiveToolBindings(Handle<GraphHive> hive) :
	_hive{hive}
{
	_registerBinding("getNodeNames");
	_registerBinding("getNodeId");
}

std::vector<ModelToolDefinition> BasicHiveToolBindings::getModelToolDefinitions()
{
	std::vector<ModelToolDefinition> definitions;

	definitions.push_back(ModelToolDefinition(
		"getNodeNames",
		"Get the names of all the nodes currently in the hive.",
		{},
		ModelToolDefinitionParameter("nodeNames", "Names of all the nodes in the hive.",
			ModelToolDefinitionParameter::ArrayType{ModelToolDefinitionParameter::PrimitiveType::STRING})));

	definitions.push_back(ModelToolDefinition(
		"getNodeId",
		"Get the id of a node in the hive, given its name.",
		{
			ModelToolDefinitionParameter("nodeName", "Name of the node to find.",
				ModelToolDefinitionParameter::PrimitiveType::STRING)
		},
		ModelToolDefinitionParameter("nodeId", "Id of the node.",
			ModelToolDefinitionParameter::PrimitiveType::INTEGER)));

	return definitions;
}

ModelToolCallParameterValue BasicHiveToolBindings::processBinding(std::string name,
	std::vector<ModelToolCallParameterValue> parameterValues)
{
	if(name == "getNodeNames")
	{
		return ModelToolCallParameterValue("nodeNames", getNodeNames());
	}
	else if(name == "getNodeId")
	{
		std::string nodeName = std::get<std::string>(
			_getParameterValue(parameterValues, "nodeName").getValue());

		return ModelToolCallParameterValue("nodeId", getNodeId(nodeName));
	}
	else
	{
		throw AgentException(AgentException::BINDING_NOT_FOUND);
	}
}

std::vector<std::string> BasicHiveToolBindings::getNodeNames()
{
	return _hive.getInstance() -> getNodeNames();
}

long long BasicHiveToolBindings::getNodeId(std::string nodeName)
{
	Handle<GraphNode> node = _hive.getInstance() -> getNode(nodeName);

	if(!node.isValid())
	{
		throw AgentException(AgentException::NODE_NOT_FOUND);
	}

	return node.getInstance() -> getId();
}
