#ifndef AGENT_ACTION_TARGET_H
#define AGENT_ACTION_TARGET_H

#include <vector>

#include "../../agent/AgenticHarness.hpp"
#include "../../util/Handle.hpp"
#include "ActionTarget.hpp"

class ModelToolBindings;

/**
 * Action target to use for a node that can be driven by per-node agentic prompts.
 */
class AgentActionTarget : virtual public ActionTarget
{
    public:

        virtual ~AgentActionTarget() {}

		AgentActionTarget() {}

		/**
		 * Get the tool bindings this target makes available to the agentic request applied to it.
		 * @param capability Capability of the model the tool bindings are being requested for.
		 * @param serial Serial number of the action driving the request, passed through to any bindings that
		 *        apply their effect against a serialised target.
		 * @note This is intended to be on a per node instance level, not the node type level and exists to support
		 *       script defined bindings.
		 * @returns The tool bindings.
		 */
		virtual std::vector<Handle<ModelToolBindings>> getModelToolBindings(AgenticHarness::Capability capability,
			unsigned serial) = 0;

	protected:

    private:
};

#endif
