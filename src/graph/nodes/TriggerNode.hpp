#ifndef TRIGGER_NODE_H
#define TRIGGER_NODE_H

#include <string>
#include <vector>

#include "../actionTargets/AgentActionTarget.hpp"
#include "../../util/Handle.hpp"
#include "ScriptNode.hpp"

class ModelToolBindings;

/**
 * Graph node that combines ScriptNode's Lua scripting with an emitTrigger tool binding, letting an AI model
 * emit a trigger action from this node against the rest of the graph.
 */
class TriggerNode : public ScriptNode, public AgentActionTarget
{
    public:

		/**
		 * @param coreScript Main Lua source code that this node runs when invoked.
		 * @param pokeScript The script that is called for processing a poke.
		 * @param emitTriggerOnPoke If true, poking this node emits an unrestricted trigger action from it once
		 *        its poke script has run. If false, a poke runs the poke script alone, leaving emission to the
		 *        script's own trigger() calls or to a model's emitTrigger tool.
		 * @note The poke script runs on every poke regardless of this flag.
		 */
        TriggerNode(const std::string& coreScript, const std::string& pokeScript, bool emitTriggerOnPoke = true);

		GraphNodeType getType() override;

		/**
		 * Get whether poking this node emits a trigger action of its own.
		 */
		bool getEmitTriggerOnPoke();

		// Agent target API point.
		std::vector<Handle<ModelToolBindings>> getModelToolBindings(AgenticHarness::Capability capability,
			unsigned serial) override;

		AgentActionTarget* getAgentActionTarget() override;

	protected:

		// Ref counted.
        virtual ~TriggerNode();

		void _poked(GraphPoke poke) override;

	private:

        // Do not allow copying.
        TriggerNode(const TriggerNode& copyFrom);
        TriggerNode& operator= (const TriggerNode& copyFrom);

		/// Whether poking this node emits a trigger action of its own, on top of running the poke script.
		bool _emitTriggerOnPoke;
};

#endif
