#ifndef EVENT_EMITTER_H
#define EVENT_EMITTER_H

#include <list>

#include "../thread/thread.hpp"
#include "../thread/ThreadMutex.hpp"
#include "EventListener.hpp"

template<typename T> class EventListener;

/**
 * Generic event emitter.
 * @tparam T The concrete type of the listeners that will listen on this emitter. It is required that the implementer
 *         of this event listener will also implement this type T.
 */
template<typename T> class EventEmitter
{
	friend EventListener<T>;

	public:

		EventEmitter()
		{
		}

		virtual ~EventEmitter()
		{
			std::list<EventListener<T>*> listeners = _getListeners();

			for(EventListener<T>* listener : listeners)
			{
				listener -> __removeEmitter(this);
			}
		}

		/**
		 * Add an even listener to this emitter.
		 */
		void addListener(EventListener<T>* listener)
		{
			{ SYNC(_lock)

				_boundListeners.push_back(listener);
			}

			listener -> __addEmitter(this);
		}

	protected:

		/**
		 * Get a copy of the list of the event listeners bound to this emitter.
		 * @note This is thread safe.
		 */
		std::list<EventListener<T>*> _getListeners()
		{
			std::list<EventListener<T>*> listeners;

			{ SYNC(_lock)

				listeners = _boundListeners;
			}

			return listeners;
		}

	private:

		/// Event listeners that are bound to this emitter.
		std::list<EventListener<T>*> _boundListeners;

		/// Generic lock that guards listeners.
		ThreadMutex _lock;

		/**
		 * Remove a listener from this emitter.
		 */
		void __removeListener(EventListener<T>* listener)
		{
			{ SYNC(_lock)

				_boundListeners.remove(listener);
			}
		}
};

#endif
