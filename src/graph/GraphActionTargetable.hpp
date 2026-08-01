#ifndef GRAPH_ACTION_TARGETABLE_H
#define GRAPH_ACTION_TARGETABLE_H

#include "actionTargets/AgentActionTarget.hpp"
#include "actionTargets/AgentVisibleActionTarget.hpp"
#include "actionTargets/AnimateActionTarget.hpp"
#include "actionTargets/PingActionTarget.hpp"
#include "actionTargets/SceneActionTarget.hpp"
#include "actionTargets/StrobeActionTarget.hpp"
#include "actionTargets/ScriptActionTarget.hpp"
#include "actionTargets/SerialisableActionTarget.hpp"
#include "actionTargets/TriggerActionTarget.hpp"
#include "actionTargets/VersionActionTarget.hpp"

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
		 * @note This only returns true if this can be a target for all of the actions required targetable
		 *       interfaces or if no required interfaces are specified, at least one of the optional ones.
		 */
		bool canActionTarget(GraphAction* action);

		/**
		 * Determine whether an individual targetable interface is supported by this.
		 */
		bool hasActionTarget(unsigned long actionFlag);

		/**
		 * Get action flags of actions this target supports.
		 */
		unsigned long getActionFlags();

		/// Get the target for the ping action.
		virtual PingActionTarget* getPingActionTarget();

		/// Get the target for the serialisable action.
		virtual SerialisableActionTarget* getSerialisableActionTarget();

		/// Get the target for the script action.
		virtual ScriptActionTarget* getScriptActionTarget();

		/// Get the target for the scene action.
		virtual SceneActionTarget* getSceneActionTarget();

		/// Get the target for the strobe action.
		virtual StrobeActionTarget* getStrobeActionTarget();

		/// Get the target for the animate action.
		virtual AnimateActionTarget* getAnimateActionTarget();

		/// Get the target for the version action.
		virtual VersionActionTarget* getVersionActionTarget();

		/// Get the target for the agent action.
		virtual AgentActionTarget* getAgentActionTarget();

		/// Get the target for the trigger action.
		virtual TriggerActionTarget* getTriggerActionTarget();

		/// Get the target for marking that an agentic action is being applied.
		virtual AgentVisibleActionTarget* getAgentVisibleActionTarget();

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

		/// Flags that determine whether an action can target this targetable.
		std::atomic<unsigned long> _actionFlags{0};
};

#endif
