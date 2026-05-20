#ifndef THREAD_CONDITION_PTHREAD_H
#define THREAD_CONDITION_PTHREAD_H

#include <pthread.h>
#include <string>

/**
 * PThread version of thread condition.
 */
class ThreadConditionPthread
{
    public:

        virtual ~ThreadConditionPthread();
        ThreadConditionPthread();

		void lockMutex();

		void unlockMutex();

        void wait();

        bool waitTimeout(unsigned int timeOut);

        void signal();

        void broadcast();

    private:

        // It does not make sense to copy a condition (in fact it is dangerous) so do not allow it.
        ThreadConditionPthread(const ThreadConditionPthread& copyFrom);
        ThreadConditionPthread& operator= (const ThreadConditionPthread& copyFrom);

		/** Actual condiiton to wait on. */
        pthread_cond_t _condition;

        /**
		 * A pthread based condition can have only _one_ mutex defined for all operations on it.
         * From the man page:
         * The   effect   of   using   more   than   one   mutex   for  concurrent
         * pthread_cond_timedwait() or pthread_cond_wait() operations on the  same
         * condition  variable is undefined.
		 */
        pthread_mutex_t _mutex;

		/** Append mutex lock specific error string to given string. */
		void appendPthreadMutexLockError(int errorCode, std::string& errorString);

		/** Append mutex unlock specific error string to given string. */
		void appendPthreadMutexUnlockError(int errorCode, std::string& errorString);
};

#endif
