#include "AgentAction.hpp"

#include "../../agent/ModelContext.hpp"
#include "../../agent/ModelToolBindings.hpp"
#include "../actionTargets/AgentActionTarget.hpp"
#include "../actionTargets/AgentAffectActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHive.hpp"
#include "../GraphNode.hpp"
#include "../../log/log.hpp"
#include "../../thread/thread.hpp"

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
			_context.getInstance() -> addTemporaryToolBindings(
				agentTarget -> getModelToolBindings(_capability, getId()));
		}

		// The request below block for as long as the model takes to answer, which is long enough for the
		// surface to be repopulated several times over, so anything the node only shows while it is being
		// worked on is revealed for exactly that window.
		AgentAffectActionTarget* agentAffectTarget = target -> getAgentAffectActionTarget();

		if(agentAffectTarget)
		{
			agentAffectTarget -> agentAffectingStart(true);
			agentAffectTarget -> agentPrompt(match -> prompt);
		}

		// Only called while the request below is being serviced, so it need live no longer than that.
		ProgressRelay relay(*this, agentAffectTarget);

		LOG(Logger::LogLevel::DEBUG, "Processing node agentic request.")

		try
		{
			_context = hive.getInstance() -> processNodeAgenticRequest(_capability, match -> prompt, _context,
				&relay);
		}
		catch(...)
		{
			// A failed request still has to put the node back, otherwise it would be left looking like it is
			// being worked on for the rest of the hive's life.
			if(agentAffectTarget) agentAffectTarget -> agentAffectingEnd(true);

			throw;
		}

		if(agentAffectTarget)
		{
			agentAffectTarget -> agentResponse(_context.getInstance() -> getLastResponse());
			agentAffectTarget -> agentAffectingEnd(true);
		}

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

std::vector<std::string> AgentAction::getProgress()
{
	{ SYNC(_progressLock)

		return _progress;
	}
}

void AgentAction::__receiveProgress(std::string text, AgentAffectActionTarget* target)
{
	{ SYNC(_progressLock)

		_progress.push_back(text);
	}

	if(target) target -> agentProgress(text);
}

AgentAction::ProgressRelay::ProgressRelay(AgentAction& action, AgentAffectActionTarget* target) :
	_action{action}, _target{target}
{
}

void AgentAction::ProgressRelay::agentRequestProgressReport(std::string text)
{
	_action.__receiveProgress(text, _target);
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
