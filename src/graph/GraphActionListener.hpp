#ifndef GRAPH_ACTION_LISTENER_H
#define GRAPH_ACTION_LISTENER_H

template<typename T> class EventEmitter;

/**
 * Provides ability to listen to graph action events.
 */
class GraphActionListener
{
	public:

		/**
		 * Events that a GraphAction can emit to its bound listeners.
		 */
		enum class Event
		{
			/// The action has completed.
			ACTION_COMPLETE
		};

		GraphActionListener() {}

		virtual ~GraphActionListener() {}

		/**
		 * Receive an event emitted by a bound GraphAction, dispatching it to the corresponding named hook.
		 * @param emitter The event emitter that is emitting this event.
		 */
		void emitEvent(EventEmitter<GraphActionListener>& emitter, Event event)
		{
			switch(event)
			{
				case Event::ACTION_COMPLETE: actionComplete(emitter); break;
			}
		}

		/**
		 * Indicate that the bound action has completed.
		 * @param emitter The event emitter that is emitting this event.
		 */
		virtual void actionComplete(EventEmitter<GraphActionListener>& emitter) = 0;

	protected:

	private:
};

#endif
