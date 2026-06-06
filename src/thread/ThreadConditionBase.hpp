#ifndef THREAD_CONDITION_BASE_H
#define THREAD_CONDITION_BASE_H

/**
 * Interface for a thread condition.
 * Allows thread to sleep and be woken at a later time.
 * Think of this as protecting a single variable where we follow the "condiiton" of it.
 */
class ThreadConditionBase
{
    public:

        virtual ~ThreadConditionBase();
        ThreadConditionBase();

		/**
		 * Lock the conditions mutex.
		 * @throws ThreadException
		 */
		virtual void lockMutex() = 0;

		/**
		 * Unlock the conditions mutex.
		 * @throws ThreadException
		 */
		virtual void unlockMutex() = 0;

        /**
         * Indefinitely wait on a thread condition.
		 * @warning The conditions mutex must be locked prior to calling this and unlocked after returning from this call.
		 * @throws ThreadException
         */
		virtual void wait() = 0;

        /**
         * Wait on a thread condition for up to a specified period.
		 * @warning The conditions mutex must be locked prior to calling this and unlocked after returning from this call.
         * @param timeOut Maximum period in ms to wait on condition to be signalled.
         * @returns True if timeout was triggered. False otherwise.
		 * @throws ThreadException
         */
		virtual bool waitTimeout(unsigned int timeOut) = 0;

        /**
         * Signal condition to wake a single waiting thread.
         */
		virtual void signal() = 0;

        /**
         * Signal condition to wake all waiting threads.
         */
		virtual void broadcast() = 0;

    private:

        // It does not make sense to copy a condition (in fact it is dangerous) so do not allow it.
        ThreadConditionBase(const ThreadConditionBase& copyFrom);
        ThreadConditionBase& operator= (const ThreadConditionBase& copyFrom);
};

#endif
