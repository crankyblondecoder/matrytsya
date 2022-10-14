/** Copyright Scott A.E. Lanham, Australia */

#ifndef THREAD_CONDITION_H
#define THREAD_CONDITION_H

#include <pthread.h>
#include <string>

#include "ThreadMutex.hpp"

/**
 * Allows thread to sleep and be woken at a later time.
 */
class ThreadCondition
{
    public:

        virtual ~ThreadCondition();
        ThreadCondition();

		/**
		 * Lock the conditions mutex.
		 * @throw ThreadException
		 */
		void lockMutex();

		/**
		 * Unlock the conditions mutex.
		 * @throw ThreadException
		 */
		void unlockMutex();

        /**
         * Indefinitely wait on a thread condition.
		 * @warning The conditions mutex must be locked prior to calling this and unlocked after returning from this call.
		 * @throw ThreadException
         */
        void wait();

        /**
         * Wait on a thread condition for up to a specified period.
         * @param timeOut Maximum period in ms to wait on condition to be signalled.
         * @returns True if timeout was triggered. False otherwise.
		 * @warning The conditions mutex must be locked prior to calling this and unlocked after returning from this call.
		 * @throw ThreadException
         */
        bool waitTimeout(unsigned int timeOut);

        /**
         * Signal condition to wake a single waiting thread.
         */
        void signal();

        /**
         * Signal condition to wake all waiting threads.
         */
        void broadcast();

    private:

        // It does not make sense to copy a condition (in fact it is dangerous) so do not allow it.
        ThreadCondition(const ThreadCondition& copyFrom);
        ThreadCondition& operator= (const ThreadCondition& copyFrom);

        pthread_cond_t _condition;

        // A pthread based condition can have only _one_ mutex defined for all operations on it.
        // From the man page:
        // The   effect   of   using   more   than   one   mutex   for  concurrent
        // pthread_cond_timedwait() or pthread_cond_wait() operations on the  same
        // condition  variable is undefined.
        pthread_mutex_t _mutex;

		/** Append mutex lock specific error string to given string. */
		void appendPthreadMutexLockError(int errorCode, std::string& errorString);

		/** Append mutex unlock specific error string to given string. */
		void appendPthreadMutexUnlockError(int errorCode, std::string& errorString);
};

#endif