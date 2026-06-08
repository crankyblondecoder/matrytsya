#ifndef GRAPH_ACTION_TARGETABLE_H
#define GRAPH_ACTION_TARGETABLE_H

class GraphAction;

#include "./actions/PingAction.hpp"

/**
 * Base class of all classes that can be action targets.
 * @note ALWAYS inherit this virtually.
 */
class GraphActionTargetable
{
    public:

		/**
		 * Determine whether an action can target this.
		 */
		bool canActionTarget(GraphAction* action);

		/**
		 * Apply a graph action to this target.
		 */
		void applyAction(GraphAction* action);

	protected:

		virtual ~GraphActionTargetable();

		// It makes no sense for this to be instantiable by itself.
		GraphActionTargetable();

		/**
		 * Add an action flag to the supported action flags of this target.
		 * @param actionFlag Action flag from action flag register.
		 */
		virtual void _addActionFlag(unsigned long actionFlag);

		/**
		 * Apply a ping action to this target.
		 */
		virtual void _applyAction(PingAction* action);

	private:

		unsigned long _actionFlags;
};

#endif
