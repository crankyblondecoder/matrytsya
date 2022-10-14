/** Copyright Scott A.E. Lanham, Australia */

#ifndef THREAD_MUTEX_H
#define THREAD_MUTEX_H

#include <pthread.h>

/**
 * Thread mutex used for locking.
 */
class ThreadMutex
{
    public:

        virtual ~ThreadMutex();
        ThreadMutex();

        /**
         * Lock the mutex and block until it can be locked if already locked.
         * @returns True if successfully locked.
		 * @throws ThreadException
         */
        bool lock();

        /**
         * Try and lock the mutex but do not block if cannot be affected.
         * @return true if successfully locked.
         */
        bool tryLock();

        /**
         * Unlock the mutex. It must be owned by the calling thread.
         * @return true
         */
        bool unlock();

    private:

        // It does not make sense to copy a mutex so do not allow it.
        ThreadMutex(const ThreadMutex& copyFrom);
        ThreadMutex& operator= (const ThreadMutex& copyFrom);

        pthread_mutex_t _mutex;
};

#endif