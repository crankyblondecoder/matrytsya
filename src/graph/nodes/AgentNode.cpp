#include "AgentNode.hpp"

#include "../graphActionFlagRegister.hpp"
#include "../GraphPoke.hpp"
#include "../../log/log.hpp"
#include "../../util/Handle.hpp"

AgentNode::~AgentNode()
{
}

AgentNode::AgentNode(AgenticHarness::Capability capability, std::vector<AgentAction::NodePrompt> prompts,
	bool autoTriggerAgentAction, bool serialiseEmittedActions)
	: GraphSerialisedActionNode(serialiseEmittedActions), _capability(capability), _prompts(prompts),
	  _autoTriggerAgentAction(autoTriggerAgentAction)
{
	_setEnergyCost(1);

	// Supports trigger action.
	_addActionFlag(TRIGGER_GRAPH_ACTION);
}

GraphNodeType AgentNode::getType()
{
	return GraphNodeType::AGENT_NODE;
}

AgentAction* AgentNode::emitAgentAction(bool wait)
{
	Handle<GraphNode> handle(this);

	// Action will self delete once complete.
	AgentAction* action = new AgentAction(handle, _capability, _prompts);

	action -> incrRef();

	LOG(Logger::LogLevel::DEBUG, "Emitting agent action.")

	_emitAction(action);

	if(wait) action -> waitOnComplete(0);

	return action;
}

void AgentNode::trigger()
{
	if(!_autoTriggerAgentAction) return;

	LOG(Logger::LogLevel::DEBUG, "Agent node triggered.")

	AgentAction* action = emitAgentAction(false);

	action -> decrRef();
}

TriggerActionTarget* AgentNode::getTriggerActionTarget()
{
	return this;
}

AgenticHarness::Capability AgentNode::getCapability()
{
	return _capability;
}

const std::vector<AgentAction::NodePrompt>& AgentNode::getPrompts()
{
	return _prompts;
}

bool AgentNode::getAutoTriggerAgentAction()
{
	return _autoTriggerAgentAction;
}

void AgentNode::_poked(GraphPoke poke)
{
}
