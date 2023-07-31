#ifndef REF_COUNTED_H
#define REF_COUNTED_H

#include "../thread/thread.hpp"

/**
 * Reference counted base class.
 * Used to reference count and auto-delete class instances.
 * @note References are _not_ automatically inferred and have to be managed manually.
 * @note Every occurance where a pointer to a subclass of this is stored must have obtained a ref incr prior to storing it.
 * With the exeception of the creating class which can use the automatic reference count added during construction.
 * @note For this to successfully delete when the reference count goes to zero, the destructor of the implementing class must
 * be virtual.
 */
class RefCounted
{
	public:

		/**
		 * Attempt to obtain a reference lock.
		 * @returns True if reference could be obtained. False otherwise.
		 */
		bool incrRef()
		{
			bool success = false;

			{ SYNC(_lock)

				if(!_deleting)
				{
					_refCount++;
					success = true;
				}
			}

			return success;
		}

		/**
		 * Remove a reference lock.
		 */
		void decrRef()
		{
			bool invokeDelete = false;

			{ SYNC(_lock)

				if((--_refCount) == 0)
				{
					_deleting = true;
					invokeDelete = true;
				}
			}

			if(invokeDelete) delete(this);
		}

	protected:

		/**
		 * The ref count is incremented during construction to automatically account for the pointer created.
		 * This is done here so that there is a safeguard against delete being invoked in the constructor of a subclass.
		 */
		RefCounted()
		{
			_refCount = 1;
			_deleting = false;
		}

		virtual ~RefCounted(){}

	private:

		int _refCount;
		bool _deleting;
		bool _deleted;

        ThreadMutex _lock;
};

#endif