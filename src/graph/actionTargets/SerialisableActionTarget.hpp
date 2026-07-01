#ifndef SERIALISABLE_ACTION_TARGET_H
#define SERIALISABLE_ACTION_TARGET_H

#include "../actions/SerialisableActionPayload.hpp"

/**
 * Action target to use for a node that can (de)serialise an action.
 */
class SerialisableActionTarget
{
    public:

        virtual ~SerialisableActionTarget() {}

		SerialisableActionTarget() {}

		virtual void send(SerialisableActionPayload& payload) = 0;

	protected:

    private:
};

#endif
