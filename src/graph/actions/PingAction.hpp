#ifndef PING_ACTION_H
#define PING_ACTION_H

#include "../actionTargets/PingActionTarget.hpp"
#include "../GraphAction.hpp"

/**
 * Ping graph action.
 */
class PingAction : public GraphAction
{
    public:

        virtual ~PingAction();

		PingAction(GraphNodeHandle& initNode);

		unsigned long getFlag() override;

		/**
		 * Get the current ping count from the action.
		 */
		unsigned getPingCount();

		/**
		 * Apply this action to a ping target.
		 */
		void apply(PingActionTarget* target);

	protected:

		void _complete() override;

    private:

		unsigned _pingCount;

		unsigned _debugCount = 0;
};

#endif
