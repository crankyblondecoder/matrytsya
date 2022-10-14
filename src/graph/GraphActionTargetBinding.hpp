#ifndef TEST_ACTION_TARGET_BINDING_H
#define TEST_ACTION_TARGET_BINDING_H

#include "GraphAction.hpp"

/**
 * Class to bind GraphAction to concrete GraphActionTarget.
 */
template <class ActionTargetClass> class GraphActionTargetBinding : public GraphAction
{
    public:

        virtual ~GraphActionTargetBinding();

		GraphActionTargetBinding();

		virtual unsigned long getFlag();

		virtual void apply(GraphNode*);

	protected:

		virtual void apply(ActionTargetClass*) = 0;

    private:
};

template <class ActionTargetClass> GraphActionTargetBinding<ActionTargetClass>::~GraphActionTargetBinding()
{
}

template <class ActionTargetClass> GraphActionTargetBinding<ActionTargetClass>::GraphActionTargetBinding()
{
}

template <class ActionTargetClass> unsigned long GraphActionTargetBinding<ActionTargetClass>::getFlag()
{
	return ActionTargetClass::ACTION_FLAG;
}

template <class ActionTargetClass> void GraphActionTargetBinding<ActionTargetClass>::apply(GraphNode* targetable)
{
	// This just marshals the action target into the required subclass that the action implementation can deal with.
	apply((ActionTargetClass*) targetable);
}

#endif
