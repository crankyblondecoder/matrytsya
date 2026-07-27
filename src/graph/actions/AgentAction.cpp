#include "AgentAction.hpp"

#include "../../agent/ModelContext.hpp"
#include "../../agent/ModelToolBindings.hpp"
#include "../actionTargets/AgentActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHive.hpp"
#include "../GraphNode.hpp"

#include <string>

AgentAction::~AgentAction()
{
}

AgentAction::AgentAction(Handle<GraphNode> initNode, AgenticHarness::Capability capability, std::vector<NodePrompt> prompts)
	: GraphAction(initNode, _startingEnergy), _capability(capability), _prompts(prompts), _context(0)
{
	_addFlag(AGENT_GRAPH_ACTION, true);
}

void AgentAction::_apply(GraphNode* target)
{
	const NodePrompt* match = __findPrompt(target);

	if(!match) return;

	Handle<GraphHive> hive = target -> getHive();

	if(!hive.isValid()) return;

	if(!_context.isValid())
	{
		_context = hive.getInstance() -> createNodeModelContext(_capability);
	}

	AgentActionTarget* agentTarget = target -> getAgentActionTarget();

	if(agentTarget)
	{
		_context.getInstance() -> clearTemporaryToolBindings();
		_context.getInstance() -> addTemporaryToolBindings(agentTarget -> getModelToolBindings());
	}

	_context = hive.getInstance() -> processNodeAgenticRequest(_capability, match -> prompt, _context);

	// Bindings only apply to the node the request was made for, so they must not linger for whatever
	// comes next in this context.
	if(agentTarget) _context.getInstance() -> clearTemporaryToolBindings();
}

bool AgentAction::_starting()
{
	return true;
}

void AgentAction::_complete()
{
}

Handle<ModelContext> AgentAction::getModelContext()
{
	return _context;
}

const AgentAction::NodePrompt* AgentAction::__findPrompt(GraphNode* target)
{
	std::string name = target -> getName();
	GraphNode::Type type = target -> getType();

	for(const NodePrompt& entry : _prompts)
	{
		if(entry.nodeType != type) continue;

		if(!entry.nodeIdentifier.empty() && entry.nodeIdentifier != name) continue;

		return &entry;
	}

	return 0;
}
