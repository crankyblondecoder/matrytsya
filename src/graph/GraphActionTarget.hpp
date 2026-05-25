#ifndef GRAPH_ACTION_TARGET_H
#define GRAPH_ACTION_TARGET_H

/**
 * Templated base class of all interfaces that define a specific graph action target.
 * A target is the interface used for the action to invoke operations on a graph node.
 */
template <unsigned long ActionFlag> class GraphActionTarget
{
    public:

        virtual ~GraphActionTarget();

		GraphActionTarget();

		/** Flag of action this target interface applies to. */
		static unsigned long ACTION_FLAG;

	protected:

		/**
		 * Add the action flag this action target corresponds to.
		 */
		virtual void _addActionFlag(unsigned long actionFlag) = 0;

		/**
		 * Initialise this action target.
		 * @note Cannot be called in the sub-class constructor.
		 */
		virtual void _init();

    private:
};

template <unsigned long ActionFlag> unsigned long GraphActionTarget<ActionFlag>::ACTION_FLAG = ActionFlag;

template <unsigned long ActionFlag> GraphActionTarget<ActionFlag>::~GraphActionTarget()
{
}

template <unsigned long ActionFlag> GraphActionTarget<ActionFlag>::GraphActionTarget()
{
}

template <unsigned long ActionFlag> void GraphActionTarget<ActionFlag>::_init()
{
	_addActionFlag(GraphActionTarget<ActionFlag>::ACTION_FLAG);
}

#endif
