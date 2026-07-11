#ifndef ACTION_FACTORY_H
#define ACTION_FACTORY_H

#include "../GraphHandle.hpp"

class GraphNode;
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
         * @returns Newly allocated action with its data fully deserialised. The caller is responsible for starting
		 *          the action.
         * @throws GraphException(UNKNOWN_ACTION_TYPE) if the payload carries an unrecognised action type.
         */
        static SerialisableAction* create(GraphHandle<GraphNode>& initNode, SerialisableActionPayload& payload);

    private:

        // Not instantiable.
        ActionFactory();
        ActionFactory(const ActionFactory& copyFrom);
        ActionFactory& operator= (const ActionFactory& copyFrom);
};

#endif
