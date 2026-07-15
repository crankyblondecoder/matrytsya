#ifndef GRAPH_HANDLE_H
#define GRAPH_HANDLE_H

/**
 * Handle to a reference counted graph object.
 * Guarantees that the object this references will be available while an instance of this class exists.
 * Objects will be automatically ref'd/de-ref'd.
 * @note This is not re-entrant.
 * @note T must be a reference counted class, i.e. it must inherit from RefCounted and provide incrRef()/decrRef().
 */
template <typename T> class GraphHandle
{
    public:

		/**
		 * Construct a handle from the given instance.
		 * @param instance Instance that handle refers to. May be null.
		 */
		GraphHandle(T* instance)
		{
			if(instance && instance -> incrRef())
			{
				_referencedInstance = instance;
			}
			else
			{
				_referencedInstance = 0;
			}
		}

		/**
		 * Create a new handle from another handle.
		 */
        GraphHandle(const GraphHandle<T>& copyFrom)
		{
			if(copyFrom._referencedInstance && copyFrom._referencedInstance -> incrRef())
			{
				_referencedInstance = copyFrom._referencedInstance;
			}
			else
			{
				_referencedInstance = 0;
			}
		}

		/**
		 * Handles can be re-assigned.
		 */
        GraphHandle<T>& operator= (const GraphHandle<T>& copyFrom)
		{
			if(this == &copyFrom) return *this;

			if(_referencedInstance) _referencedInstance -> decrRef();

			if(copyFrom._referencedInstance && copyFrom._referencedInstance -> incrRef())
			{
				_referencedInstance = copyFrom._referencedInstance;
			}
			else
			{
				_referencedInstance = 0;
			}

			return *this;
		}

		~GraphHandle()
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
		bool operator== (const GraphHandle<T>& compareTo) const
		{
			return _referencedInstance == compareTo._referencedInstance;
		}

		/**
		 * Compare two handles for inequality.
		 * @param compareTo Handle to compare against.
		 * @returns True if the handles reference different instance pointers.
		 */
		bool operator!= (const GraphHandle<T>& compareTo) const
		{
			return _referencedInstance != compareTo._referencedInstance;
		}

		/**
		 * Clear the handle, i.e. De-reference the pointed to instance and make this handle invalid.
		 */
		void clear()
		{
			if(_referencedInstance) _referencedInstance -> decrRef();

			_referencedInstance = 0;
		}

    protected:

    private:

		T* _referencedInstance;
};

#endif
