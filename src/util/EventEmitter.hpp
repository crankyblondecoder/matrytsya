#ifndef EVENT_EMITTER_H
#define EVENT_EMITTER_H

#include <list>

#include "../thread/thread.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../util/CastHandle.hpp"

#include "EventListener.hpp"

/**
 * Generic event emitter.
 * @tparam T The concrete type of the listeners that will listen on this emitter.
 * @note This class relies on not being reachable immediately before destruction. For example, if all sub-classes
 *       of this are ref counted.
 */
template<typename T> class EventEmitter
{
	public:

		EventEmitter()
		{
		}

		virtual ~EventEmitter()
		{
			{ SYNC(_lock)

				_deleting = true;
			}

			_boundListeners.clear();
		}

		/**
		 * Binding to an event listener.
		 * The idea of this class is that it handles pointer availability for the concrete listener.
		 * @note This is a value type. It is stored and passed by value, so it is deliberately not
		 *       polymorphic; a derived binding would be sliced on every copy into the binding list.
		 */
		class EventListenerBinding
		{
			public:

				EventListenerBinding(CastHandle<T>& listenerHandle) : _safeHandle(listenerHandle) {}

				/**
				 * Emit event. The meaning of the event is specifc to the implementer.
				 * @param emitter The event emitter that is emitting this event.
				 */
				void emitEvent(EventEmitter<T>& emitter, T::Event event)
				{
					if(_safeHandle.isValid()) _safeHandle.getInstance() -> emitEvent(emitter, event);
				}

				/**
				 * Whether the given event listener is bound by this binding.
				 */
				bool isBound(EventListener<T>& listener)
				{
					return _safeHandle.isValid() && _safeHandle.getInstance() == &listener;
				}

				/**
				 * Get whether this binding has a valid handle to a listener.
				 */
				bool isValid()
				{
					return _safeHandle.isValid();
				}

			private:

				CastHandle<T> _safeHandle;
		};

		/**
		 * Emit an event. This will trigger the event on the listener.
		 */
		void emitEvent(T::Event event)
		{
			std::list<EventListenerBinding> bindingsCopy;

			{ SYNC(_lock)

				if(_deleting) return;

				// Should be a deep copy.
				bindingsCopy = _boundListeners;
			}

			for(EventListenerBinding& binding : bindingsCopy)
			{
				binding.emitEvent(*this, event);
			}
		}

		/**
		 * Add an event listener to this emitter.
		 * It is the responsibility of the caller to remove the listener when no longer required.
		 */
		void addListener(EventListener<T>& listener)
		{
			{ SYNC(_lock)

				if(_deleting) return;
			}

			EventListenerBinding binding = listener.getEventListenerBinding();

			if(binding.isValid())
			{
				{ SYNC(_lock)

					if(_deleting) return;

					_boundListeners.push_back(binding);
				}
			}
		}

		/**
		 * Remove a listener.
		 */
		void removeListener(EventListener<T>& listener)
		{
			// Splice the found bindings out into a local list so that their destructors (which may decrRef
			// and delete the ref-counted listener guard) run after _lock is released, avoiding re-entry.
			std::list<EventListenerBinding> removed;

			{ SYNC(_lock)

				if(_deleting) return;

				for(auto it = _boundListeners.begin(); it != _boundListeners.end(); )
				{
					// Splice does not invalidate the iterator, it re-links the node into removed, so the
					// iterator has to be advanced before the transfer or the walk continues into removed.
					auto current = it++;

					if(current -> isBound(listener))
					{
						removed.splice(removed.begin(), _boundListeners, current);
					}
				}
			}
		}

		/**
		 * Remove all listeners.
		 */
		void removeAllListeners()
		{
			std::list<EventListenerBinding> removed;

			{ SYNC(_lock)

				if(_deleting) return;

				removed.splice(removed.begin(), _boundListeners);
			}
		}

	protected:

	private:

		/// Generic lock.
		ThreadMutex _lock;

		/// Whether this emitter is in the process of deleting.
		bool _deleting = false;

		/// Bindings of event listeners that are bound to this emitter.
		std::list<EventListenerBinding> _boundListeners;
};

#endif
