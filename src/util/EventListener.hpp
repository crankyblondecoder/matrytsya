#ifndef EVENT_LISTENER_H
#define EVENT_LISTENER_H

#include <list>

#include "EventEmitter.hpp"

template<typename T> class EventEmitter;

/**
 * Generic event listener.
 * @tparam T The concrete type of the listener interface this event listener is limited to.
 */
template<typename T> class EventListener
{
	friend class EventEmitter<T>;

	public:

		EventListener()
		{
		}

		/// Will remove this listener from any bound emitters.
		virtual ~EventListener()
		{
			std::list<EventEmitter<T>*> emitters = _boundEmitters;

			for(EventEmitter<T>* emitter : emitters)
			{
				emitter -> __removeListener(this);
			}
		}

		/**
		 * Get the listener that implements the concrete type,
		 */
		virtual T* getListener()
		{
			return 0;
		}

	protected:

	private:

		/// Event emitters this listener is bound to.
		std::list<EventEmitter<T>*> _boundEmitters;

		/**
		 * Add an emitter that this is bound to.
		 */
		void __addEmitter(EventEmitter<T>* emitter)
		{
			_boundEmitters.push_back(emitter);
		}

		/**
		 * Remove an emitter that this is bound to.
		 */
		void __removeEmitter(EventEmitter<T>* emitter)
		{
			_boundEmitters.remove(emitter);
		}
};

#endif
