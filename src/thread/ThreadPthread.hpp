#ifndef THREAD_PTHREAD_H
#define THREAD_PTHREAD_H

#include "ThreadBase.hpp"
#include <pthread.h>

/**
 * Create and invoke thread entry point.
 */
class ThreadPthread : public ThreadBase
{
    public:

        virtual ~ThreadPthread();
        ThreadPthread();

    protected:

        bool _start(void*(*threadEntry)(void*)) override;

		void _forceStop() override;

    private:

        // It does not make sense to copy a thread so do not allow it.
        ThreadPthread(const ThreadPthread& copyFrom);
        ThreadPthread& operator= (const ThreadPthread& copyFrom);

        pthread_t _thread;
};

#endif
