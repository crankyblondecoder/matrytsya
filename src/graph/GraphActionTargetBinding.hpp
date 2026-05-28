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

		/**
		 * @param initNode Initial node the new action is bound to. This action will not be applied to this node.
		 * @param energy The energy that is assigned to the action.
		 */
		GraphActionTargetBinding(GraphNodeHandle& initNode, unsigned energy);

		virtual unsigned long getFlag() override;

	protected:

		virtual void _apply(GraphNode*) override;

		/**
		 * Marshal the generic node type that action is being applied to into the specific action target type.
		 */
		virtual void _apply(ActionTargetClass*) = 0;

    private:
};

template <class ActionTargetClass> GraphActionTargetBinding<ActionTargetClass>::~GraphActionTargetBinding()
{
}

template <class ActionTargetClass> GraphActionTargetBinding<ActionTargetClass>::GraphActionTargetBinding(
	GraphNodeHandle& initNode, unsigned energy) : GraphAction(initNode, energy)
{
}

template <class ActionTargetClass> unsigned long GraphActionTargetBinding<ActionTargetClass>::getFlag()
{
	return ActionTargetClass::ACTION_FLAG;
}

template <class ActionTargetClass> void GraphActionTargetBinding<ActionTargetClass>::_apply(GraphNode* targetable)
{
	// This just marshals the action target into the required subclass that the action implementation can deal with.
	_apply((ActionTargetClass*) targetable);
}

#endif
