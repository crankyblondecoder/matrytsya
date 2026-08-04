#include "AgentAction.hpp"

#include "../../agent/ModelContext.hpp"
#include "../../agent/ModelToolBindings.hpp"
#include "../actionTargets/AgentActionTarget.hpp"
#include "../actionTargets/AgentAffectActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHive.hpp"
#include "../GraphNode.hpp"
#include "../../log/log.hpp"

#include <string>

AgentAction::~AgentAction()
{
}

AgentAction::AgentAction(Handle<GraphNode> initNode, AgenticHarness::Capability capability,
	std::vector<NodePrompt> prompts)
	: GraphAction(initNode, _startingEnergy, 1, true), _capability(capability), _prompts(prompts), _context(0)
{
	_addFlag(AGENT_GRAPH_ACTION, true);
}

bool AgentAction::_apply(GraphNode* target)
{
	const NodePrompt* match = __findPrompt(target);

	if(!match) return false;

	Handle<GraphHive> hive = target -> getHive();

	if(hive.isValid())
	{
		if(!_context.isValid())
		{
			_context = hive.getInstance() -> createModelContext(AgenticHarness::Role::NODE, _capability);
		}

		AgentActionTarget* agentTarget = target -> getAgentActionTarget();

		if(agentTarget)
		{
			_context.getInstance() -> clearTemporaryToolBindings();
			_context.getInstance() -> addTemporaryToolBindings(agentTarget -> getModelToolBindings(_capability, getId()));
		}

		// The request below blocks for as long as the model takes to answer, which is long enough for the
		// surface to be repopulated several times over, so anything the node only shows while it is being
		// worked on is revealed for exactly that window.
		AgentAffectActionTarget* agentAffectTarget = target -> getAgentAffectActionTarget();

		if(agentAffectTarget) agentAffectTarget -> agentAffectingStart(true);

		LOG(Logger::LogLevel::DEBUG, "Processing node agentic request.")

		try
		{
			_context = hive.getInstance() -> processNodeAgenticRequest(_capability, match -> prompt, _context);
		}
		catch(...)
		{
			// A failed request still has to put the node back, otherwise it would be left looking like it is
			// being worked on for the rest of the hive's life.
			if(agentAffectTarget) agentAffectTarget -> agentAffectingEnd(true);

			throw;
		}

		if(agentAffectTarget) agentAffectTarget -> agentAffectingEnd(true);

		// Bindings only apply to the node the request was made for, so they must not linger for whatever
		// comes next in this context.
		if(agentTarget) _context.getInstance() -> clearTemporaryToolBindings();

		return match -> terminateOnResponse;
	}

	return false;
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
	GraphNodeType type = target -> getType();

	for(const NodePrompt& entry : _prompts)
	{
		if(entry.nodeType != type) continue;

		if(!entry.nodeIdentifier.empty() && entry.nodeIdentifier != name) continue;

		return &entry;
	}

	return 0;
}
