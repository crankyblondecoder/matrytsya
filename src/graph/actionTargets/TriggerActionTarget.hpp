#ifndef TRIGGER_ACTION_TARGET_H
#define TRIGGER_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a node that can be triggered.
 */
class TriggerActionTarget : virtual public ActionTarget
{
    public:

        virtual ~TriggerActionTarget() {}

		TriggerActionTarget() {}

		/** Trigger the node. */
		virtual void trigger() = 0;

	protected:

    private:
};

#endif
