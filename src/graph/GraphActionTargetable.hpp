#ifndef GRAPH_ACTION_TARGETABLE_H
#define GRAPH_ACTION_TARGETABLE_H

#include "GraphAction.hpp"

/**
 * Base class of all classes that can be action targets.
 * @note ALWAYS inherit this virtually.
 */
class GraphActionTargetable
{
    public:

	protected:

		virtual ~GraphActionTargetable();

		// It makes no sense for this to be instantiable by itself.
		GraphActionTargetable();

		/**
		 * Add an action flag to the supported action flags of this target.
		 * @param actionFlag Action flag from action flag register.
		 */
		void addActionFlag(unsigned long actionFlag);

		/**
		 * Determine whether an action can target this.
		 * Intended to fullfil GraphNode pure virtual function.
		 */
		bool canActionTarget(GraphAction*);

    private:

		unsigned long _actionFlags;
};

#endif
