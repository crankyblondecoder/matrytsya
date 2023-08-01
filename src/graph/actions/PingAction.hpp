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

		PingAction();

	protected:

		void _apply(PingActionTarget*);

    private:
};

#endif
