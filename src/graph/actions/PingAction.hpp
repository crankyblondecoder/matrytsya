#ifndef PING_ACTION_H
#define PING_ACTION_H

#include <atomic>
#include <span>

#include "../actionTargets/PingActionTarget.hpp"
#include "SerialisableAction.hpp"

/**
 * Ping graph action.
 */
class PingAction : public SerialisableAction
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

		std::span<uint8_t> _serialise() override;

		void _deserialise(std::span<uint8_t> data) override;

    private:

		std::atomic<unsigned> _pingCount;

		unsigned _debugCount = 0;
};

#endif
