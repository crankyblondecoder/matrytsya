#ifndef GRAPH_ACTION_TARGETABLE_H
#define GRAPH_ACTION_TARGETABLE_H

#include "actionTargets/PingActionTarget.hpp"
#include "actionTargets/SerialisableActionTarget.hpp"

#include <atomic>

class GraphAction;

/**
 * Base class of all classes that can be action targets.
 * @note ALWAYS inherit this virtually.
 */
class GraphActionTargetable
{
    public:

		/**
		 * Determine whether an action can target this.
		 * @note This returns true if this can be a target for at least one of the actions required targetable
		 *       interfaces, i.e. It doesn't guarantee that all targetable interfaces are supported.
		 */
		bool canActionTarget(GraphAction* action);

		/**
		 * Determine whether an individual targetable interface is supported by this.
		 */
		bool hasActionTarget(unsigned long actionFlag);

		/// Get the target for the ping action.
		virtual PingActionTarget* getPingActionTarget();

		/// Get the target for the serialisable action.
		virtual SerialisableActionTarget* getSerialisableActionTarget();

	protected:

		virtual ~GraphActionTargetable();

		// It makes no sense for this to be instantiable by itself.
		GraphActionTargetable();

		/**
		 * Add an action flag to the supported action flags of this target.
		 * @param actionFlag Action flag from action flag register.
		 */
		virtual void _addActionFlag(unsigned long actionFlag);

	private:

		std::atomic<unsigned long> _actionFlags{0};
};

#endif
