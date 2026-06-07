#ifndef PING_ACTION_H
#define PING_ACTION_H

#include "../GraphActionTargetBinding.hpp"
#include "../actionTargets/PingActionTarget.hpp"

/**
 * Ping graph action.
 */
class PingAction : public GraphActionTargetBinding<PingActionTarget>
{
    public:

        virtual ~PingAction();

		PingAction(GraphNodeHandle& initNode);

		/**
		 * Get the current ping count from the action.
		 */
		unsigned getPingCount();

	protected:

		void _apply(PingActionTarget*);

		void _complete();

    private:

		unsigned _pingCount;
};

#endif
