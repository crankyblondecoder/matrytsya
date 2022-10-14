/** Copyright Scott A.E. Lanham, Australia */

#ifndef THREAD_LOCK_H
#define THREAD_LOCK_H

#include "ThreadMutex.hpp"

/**
 *  Used for automatic unlocking of mutex.
 *  To use this, create a local instance of this class which will be destroyed on {} block exit.
 */
class ThreadLock
{
    public:

        virtual ~ThreadLock();
        ThreadLock(ThreadMutex*);

    private:

        // It does not make sense to copy a lock so do not allow it.
        ThreadLock(const ThreadLock& copyFrom);
        ThreadLock& operator= (const ThreadLock& copyFrom);

        ThreadMutex* _mutex;

        bool _locked;
};

#endif
