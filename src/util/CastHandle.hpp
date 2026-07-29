#ifndef CAST_HANDLE_H
#define CAST_HANDLE_H

#include "RefCounted.hpp"

/**
 * Handle to a reference counted object, where the referenced instance pointer and the reference counted
 * pointer used to guard its lifetime are different branches of the same concrete sub-class.
 * Guarantees that the object this references will be available while an instance of this class exists.
 * The guard is automatically ref'd/de-ref'd; the instance itself is never ref counted directly.
 * @note This is not re-entrant.
 * @note Guard must be the RefCounted branch of the same concrete object that instance points into.
 */
template <typename T> class CastHandle
{
    public:

		/**
		 * Construct a handle from the given instance, guarded by the given reference counted pointer.
		 * @param instance Instance that handle refers to. May be null.
		 * @param guard Reference counted pointer that guards the lifetime of instance. May be null.
		 */
		CastHandle(T* instance, RefCounted* guard)
		{
			if(instance && guard && guard -> incrRef())
			{
				_referencedInstance = instance;
				_guard = guard;
			}
			else
			{
				_referencedInstance = 0;
				_guard = 0;
			}
		}

		/**
		 * Create a new handle from another handle.
		 */
        CastHandle(const CastHandle<T>& copyFrom)
		{
			if(copyFrom._referencedInstance && copyFrom._guard && copyFrom._guard -> incrRef())
			{
				_referencedInstance = copyFrom._referencedInstance;
				_guard = copyFrom._guard;
			}
			else
			{
				_referencedInstance = 0;
				_guard = 0;
			}
		}

		/**
		 * Handles can be re-assigned.
		 */
        CastHandle<T>& operator= (const CastHandle<T>& copyFrom)
		{
			if(this == &copyFrom) return *this;

			if(_guard) _guard -> decrRef();

			if(copyFrom._referencedInstance && copyFrom._guard && copyFrom._guard -> incrRef())
			{
				_referencedInstance = copyFrom._referencedInstance;
				_guard = copyFrom._guard;
			}
			else
			{
				_referencedInstance = 0;
				_guard = 0;
			}

			return *this;
		}

		/**
		 * @note This is not safe to call from an external SYNC block.
		 */
		~CastHandle()
		{
			clear();
		}

		/**
		 * Get the instance contained in this handle.
		 * @returns Pointer to instance. Null if not available.
		 */
		T* getInstance()
		{
			return _referencedInstance;
		}

		/** Get whether the handle is valid. ie A valid instance pointer is available. */
		bool isValid()
		{
			return _referencedInstance != 0;
		}

		/**
		 * Compare two handles for equality.
		 * @param compareTo Handle to compare against.
		 * @returns True if both handles reference the same instance pointer.
		 */
		bool operator== (const CastHandle<T>& compareTo) const
		{
			return _referencedInstance == compareTo._referencedInstance;
		}

		/**
		 * Compare two handles for inequality.
		 * @param compareTo Handle to compare against.
		 * @returns True if the handles reference different instance pointers.
		 */
		bool operator!= (const CastHandle<T>& compareTo) const
		{
			return _referencedInstance != compareTo._referencedInstance;
		}

		/**
		 * Clear the handle, i.e. De-reference the guard instance and make this handle invalid.
		 * @note This is not safe to call from an external SYNC block due to the possibility of the referenced
		 *       instance destructor being called.
		 */
		void clear()
		{
			if(_guard) _guard -> decrRef();

			_referencedInstance = 0;
			_guard = 0;
		}

    protected:

    private:

		T* _referencedInstance;

		RefCounted* _guard;
};

#endif
