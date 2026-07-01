#ifndef ACTION_FACTORY_H
#define ACTION_FACTORY_H

#include "../GraphNodeHandle.hpp"

class SerialisableAction;
class SerialisableActionPayload;

/**
 * Factory for creating serialisable graph actions from a payload.
 */
class ActionFactory
{
    public:

        /**
         * Create a serialisable action from a payload.
         * @param initNode Initial node to bind the new action to.
         * @param payload Payload carrying the serialised action data. The type encoded in the payload determines
		 *        which concrete action is created.
         * @returns Newly allocated action with its data fully deserialised. The caller is responsible for calling
		 *          decrRef() when done.
         * @throws GraphException(UNKNOWN_ACTION_TYPE) if the payload carries an unrecognised action type.
         */
        static SerialisableAction* create(GraphNodeHandle& initNode, SerialisableActionPayload& payload);

    private:

        // Not instantiable.
        ActionFactory();
        ActionFactory(const ActionFactory& copyFrom);
        ActionFactory& operator= (const ActionFactory& copyFrom);
};

#endif
