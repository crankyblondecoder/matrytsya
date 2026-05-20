#ifndef THREAD_MUTEX_PTHREAD_H
#define THREAD_MUTEX_PTHREAD_H

#include <pthread.h>

/**
 * Pthread version of thread mutex used for locking.
 */
class ThreadMutexPthread
{
    public:

        virtual ~ThreadMutexPthread();
        ThreadMutexPthread();

        bool lock();

        bool tryLock();

        bool unlock();

    private:

        // It does not make sense to copy a mutex so do not allow it.
        ThreadMutexPthread(const ThreadMutexPthread& copyFrom);
        ThreadMutexPthread& operator= (const ThreadMutexPthread& copyFrom);

        pthread_mutex_t _mutex;
};

#endif
