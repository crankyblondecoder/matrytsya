#ifndef GRAPH_HIVE_LISTENER_H
#define GRAPH_HIVE_LISTENER_H

template<typename T> class EventEmitter;

/**
 * Provides ability to listen to graph hive events.
 */
class GraphHiveListener
{
	public:

		/**
		 * Events that a GraphHive can emit to its bound listeners.
		 */
		enum class Event
		{
			/// A node has joined the hive.
			NODE_ADDED
		};

		GraphHiveListener() {}

		virtual ~GraphHiveListener() {}

		/**
		 * Receive an event emitted by a bound GraphHive, dispatching it to the corresponding named hook.
		 * @param emitter The event emitter that is emitting this event.
		 */
		void emitEvent(EventEmitter<GraphHiveListener>& emitter, Event event)
		{
			switch(event)
			{
				case Event::NODE_ADDED: hiveNodeAdded(emitter); break;
			}
		}

		/**
		 * Indicate that a node has joined the bound hive.
		 * @param emitter The event emitter that is emitting this event.
		 * @note Emitted on the thread that added the node, once the node has joined and no hive lock is held, so
		 *       the hive can be queried from here. Which node joined is not carried; whatever it is, it is in the
		 *       hive by the time this is called.
		 */
		virtual void hiveNodeAdded(EventEmitter<GraphHiveListener>& emitter) = 0;
};

#endif
