#ifndef EVENT_LISTENER_H
#define EVENT_LISTENER_H

#include "../util/CastHandle.hpp"

template<typename T> class EventEmitter;

/**
 * Generic event listener.
 * @tparam T The concrete type of the listener interface this event listener is limited to.
 */
template<typename T> class EventListener : public T
{
	public:

		EventListener()
		{
		}

		virtual ~EventListener()
		{
		}

		/**
		 * Populate a handle that can be applied to the event listener interface.
		 */
		virtual void populateEventListenerHandle(CastHandle<T>& handle) = 0;

		/**
		 * Get the event listener binding that an event emitter expects.
		 */
		EventEmitter<T>::EventListenerBinding getEventListenerBinding()
		{
			CastHandle<T> listenerHandle(0, 0);

			populateEventListenerHandle(listenerHandle);

			return typename EventEmitter<T>::EventListenerBinding(listenerHandle);
		}

	protected:

	private:
};

#endif
