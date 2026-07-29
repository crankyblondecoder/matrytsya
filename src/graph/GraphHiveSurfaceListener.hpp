#ifndef GRAPH_HIVE_SURFACE_LISTENER_H
#define GRAPH_HIVE_SURFACE_LISTENER_H

template<typename T> class EventEmitter;

/**
 * Provides ability to listen to graph hive surface events.
 */
class GraphHiveSurfaceListener
{
	public:

		/**
		 * Events that a GraphHiveSurface can emit to its bound listeners.
		 */
		enum class Event
		{
			/// The surface has changed.
			SURFACE_CHANGED
		};

		GraphHiveSurfaceListener() {}

		virtual ~GraphHiveSurfaceListener() {}

		/**
		 * Receive an event emitted by a bound GraphHiveSurface, dispatching it to the corresponding named hook.
		 * @param emitter The event emitter that is emitting this event.
		 */
		void emitEvent(EventEmitter<GraphHiveSurfaceListener>& emitter, Event event)
		{
			switch(event)
			{
				case Event::SURFACE_CHANGED: hiveSurfaceChanged(emitter); break;
			}
		}

		/**
		 * Indicate that the bound hive surface has changed.
		 * @param emitter The event emitter that is emitting this event.
		 */
		virtual void hiveSurfaceChanged(EventEmitter<GraphHiveSurfaceListener>& emitter) = 0;

	protected:

	private:
};

#endif
