#ifndef THREAD_RESOURCE_LOCK_H
#define THREAD_RESOURCE_LOCK_H

#include <atomic>
#include <vector>

#include "ThreadCondition.hpp"

/**
 * Lock for a single shared resource.
 * @note This cannot be safely destructed if threads are still waiting on a lock, i.e. it has to be externally
 *       guarded while in use.
 */
class ThreadResourceLock
{
	public:

		virtual ~ThreadResourceLock();

		ThreadResourceLock();

		/**
		 * Block the calling thread until the resource is free, then mark it as busy.
		 * @throws ThreadException
		 */
		void lock();

		/**
		 * Mark the resource as free and wake a single thread waiting within lock().
		 * @throws ThreadException
		 */
		void unlock();

		/**
		 * Determine if current thread has the lock.
		 */
		bool hasLock();

	private:

		// It does not make sense to copy a lock so do not allow it.
		ThreadResourceLock(const ThreadResourceLock& copyFrom);
		ThreadResourceLock& operator= (const ThreadResourceLock& copyFrom);

		/// Condition guarding the resource busy flag.
		ThreadCondition _condition;

		/// True if the resource is currently held by a thread.
		bool _locked = false;

		/// Unique id of this context.
		unsigned _id;

		/// Counter used to derive each context's unique id.
		static std::atomic<unsigned> _nextId;

		/// Lock record used with static per thread data.
		struct LockRecord
		{
			/// The id of the ThreadResourceLock.
			unsigned resourceLockId;

			/// True if the calling thread is waiting on a lock.
			bool waiting;

			/// True if the calling thread has the lock.
			bool hasLock;
		};

		/// Per thread lock records.
		static inline thread_local std::vector<LockRecord> _lockRecords;

		/**
		 * Get the lock record for the current thread.
		 * @note Will create one if none exists.
		 */
		LockRecord& __getLockRecord();

		/**
		 * Wait on the lock becoming available.
		 */
		void __waitOnLock();

		/**
		 * Get whether calling thread is waiting on a lock.
		 */
		bool __waitingOnLock();

		/**
		 * Stop the thread waiting on a lock.
		 */
		void __stopWaitingOnLock();

		/**
		 * Claim the lock for the calling thread.
		 */
		void __claimLock();

		/**
		 * Get whether the calling thread already has a lock.
		 */
		bool __hasLock();

		/**
		 * Release the lock the calling thread has.
		 */
		void __releaseLock();
};

#endif
