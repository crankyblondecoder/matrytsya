#ifndef THREAD_MUTEX_BASE_H
#define THREAD_MUTEX_BASE_H

/**
 * Base class for thread mutex used for locking.
 */
class ThreadMutexBase
{
    public:

        virtual ~ThreadMutexBase();
        ThreadMutexBase();

        /**
         * Lock the mutex and block until it can be locked if already locked.
         * @returns True if successfully locked.
		 * @throws ThreadException
         */
		virtual bool lock() = 0;

        /**
         * Try and lock the mutex but do not block if cannot be affected.
         * @return true if successfully locked.
         */
		virtual bool tryLock() = 0;

        /**
         * Unlock the mutex. It must be owned by the calling thread.
         * @return true
         */
		virtual bool unlock() = 0;

    private:

        // It does not make sense to copy a mutex so do not allow it.
		ThreadMutexBase(const ThreadMutexBase& copyFrom);
        ThreadMutexBase& operator= (const ThreadMutexBase& copyFrom);
};

#endif
