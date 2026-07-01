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

		/**
		 * Send the serialised payload to wherever the payload needs to go.
		 * What that location is, is implementation specific.
		 * @param payload Payload to send.
		 * @returns True if succesfully sent. False otherwise.
		 */
		virtual bool send(SerialisableActionPayload& payload) = 0;

	protected:

    private:
};

#endif
