#ifndef GRAPH_ACTION_TARGET_H
#define GRAPH_ACTION_TARGET_H

#include "GraphActionTargetable.hpp"

/**
 * Templated base class of all interfaces that define a specific graph action target.
 * A target is the interface used for the action to invoke operations on a graph node.
 */
template <unsigned long ActionFlag> class GraphActionTarget : public virtual GraphActionTargetable
{
    public:

        virtual ~GraphActionTarget();

		GraphActionTarget();

		/** Flag of action this target interface applies to. */
		static unsigned long ACTION_FLAG;

	protected:

		/**
		 * Set the energy cost required to run an action on this target.
		 */
		void setEnergyCost(int energyCost);

    private:

		/**
		 * Energy cost of applying an action to this target.
		 * Negative indicates it will give energy to the action.
		 */
		int _energyCost;
};

template <unsigned long ActionFlag> unsigned long GraphActionTarget<ActionFlag>::ACTION_FLAG = ActionFlag;

template <unsigned long ActionFlag> GraphActionTarget<ActionFlag>::~GraphActionTarget()
{
}

template <unsigned long ActionFlag> GraphActionTarget<ActionFlag>::GraphActionTarget()
{
	addActionFlag(ActionFlag);
	_energyCost = 0;
}

template <unsigned long ActionFlag> void GraphActionTarget<ActionFlag>::setEnergyCost(int energyCost)
{
	_energyCost = energyCost;
}

#endif
