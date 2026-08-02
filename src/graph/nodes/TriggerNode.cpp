#include "TriggerNode.hpp"

#include "../actions/TriggerAction.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHive.hpp"
#include "../GraphToolBindingsFactory.hpp"
#include "../../agent/ModelToolBindings.hpp"
#include "../../util/Handle.hpp"

TriggerNode::~TriggerNode()
{
}

TriggerNode::TriggerNode(const std::string& coreScript, const std::string& pokeScript, bool emitTriggerOnPoke)
	: ScriptNode(coreScript, pokeScript), _emitTriggerOnPoke{emitTriggerOnPoke}
{
	_setEnergyCost(1);

	// Supports agent action.
	_addActionFlag(AGENT_GRAPH_ACTION);
}

GraphNode::Type TriggerNode::getType()
{
	return Type::TRIGGER_NODE;
}

std::vector<Handle<ModelToolBindings>> TriggerNode::getModelToolBindings(AgenticHarness::Capability capability,
	unsigned serial)
{
	std::vector<Handle<ModelToolBindings>> tools;

	// What the hive's factory holds for this class, which is what every trigger node offers alike.
	Handle<GraphHive> hive = getHive();

	if(hive.isValid())
	{
		Handle<GraphToolBindingsFactory> factory = hive.getInstance() -> getToolBindingsFactory();

		if(factory.isValid())
		{
			tools = factory.getInstance() -> getGraphNodeToolBindings(capability, Handle<GraphNode>(this));
		}
	}

	// On top of those, whatever this node's own core script declared. A hive with no factory set still
	// reaches this, so a script defined tool does not depend on one being there.
	for(Handle<ModelToolBindings>& scriptTool : _getScriptToolBindings(capability, serial))
	{
		tools.push_back(scriptTool);
	}

	return tools;
}

AgentActionTarget* TriggerNode::getAgentActionTarget()
{
	return this;
}

bool TriggerNode::getEmitTriggerOnPoke()
{
	return _emitTriggerOnPoke;
}

void TriggerNode::_poked(GraphPoke poke)
{
	// The poke script runs first so that anything it leaves behind is already in place by the time the
	// trigger this node emits reaches the rest of the graph.
	ScriptNode::_poked(poke);

	if(!_emitTriggerOnPoke) return;

	Handle<GraphNode> handle(this);

	// Action will self delete once complete.
	TriggerAction* action = new TriggerAction(handle);

	action -> incrRef();

	_emitAction(action);

	action -> decrRef();
}
