#ifndef AGENT_AFFECTING_ACTION_EMITTER_H
#define AGENT_AFFECTING_ACTION_EMITTER_H

/**
 * Graph node that can invoke emmission of agent affect actions.
 * @note This class is only intended to be inherited and not directly part of the graph.
 */
class AgentAffectingActionEmitter
{
    public:

	protected:

		/**
		 * @param emitAgentAffectAction Whether this emits an agent affect action if an agent action directly affects it.
		 */
        AgentAffectingActionEmitter(bool emitsAgentAffectAction);

        virtual ~AgentAffectingActionEmitter() = 0;

		/**
		 * Called when an agentic action starts being applied.
		 * @param direct True if an agent action is directly affecting the concrete subclass.
		 */
		void _agentAffectingStart(bool direct);

		/**
		 * Called when an agentic action ends being applied.
		 * @param direct True if an agent action is directly affecting the concrete subclass.
		 */
		void _agentAffectingEnd(bool direct);

		/**
		 * Subclass hook to invoke emitting of an agent affecting action.
		 * @note This exists because this mixin class can't emit actions directly.
		 */
		virtual void _emitAgentAffectAction(bool agentAffecting) = 0;

    private:

        // Do not allow copying.
        AgentAffectingActionEmitter(const AgentAffectingActionEmitter& copyFrom);
        AgentAffectingActionEmitter& operator= (const AgentAffectingActionEmitter& copyFrom);

		/// Whether this emits an agent affect action if an agent action directly affects it.
		bool _emitsAgentAffectAction;
};

#endif
