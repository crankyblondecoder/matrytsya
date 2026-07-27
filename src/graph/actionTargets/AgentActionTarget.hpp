#ifndef AGENT_ACTION_TARGET_H
#define AGENT_ACTION_TARGET_H

#include <vector>

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
		 * @returns The tool bindings.
		 */
		virtual std::vector<Handle<ModelToolBindings>> getModelToolBindings() = 0;

	protected:

    private:
};

#endif
