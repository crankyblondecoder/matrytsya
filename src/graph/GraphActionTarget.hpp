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

    private:
};

template <unsigned long ActionFlag> unsigned long GraphActionTarget<ActionFlag>::ACTION_FLAG = ActionFlag;

template <unsigned long ActionFlag> GraphActionTarget<ActionFlag>::~GraphActionTarget()
{
}

template <unsigned long ActionFlag> GraphActionTarget<ActionFlag>::GraphActionTarget()
{
	_addActionFlag(ActionFlag);
}

#endif
