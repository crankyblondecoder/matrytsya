#ifndef HIVE_PERSISTER_H
#define HIVE_PERSISTER_H

#include <map>
#include <string>
#include <vector>

#include "HiveRecorder.hpp"
#include "../thread/Thread.hpp"
#include "../thread/ThreadCondition.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../util/Handle.hpp"
#include "../util/RefCounted.hpp"

class GraphHive;
class GraphNode;
class HiveLoader;
class LmdbHiveStore;

/**
 * Keeps a running hive's stored copy in an LmdbHiveStore up to date with it, so that the hive is rebuilt as it
 * has grown rather than as it was first defined.
 *
 * What is stored is what a HiveRecorder records of the hive: each node as it joined, with its edges, build state
 * and the hive's strobe registrations as they stand. A node removed from the hive is removed from the store.
 *
 * Changes are noticed by the hive's version changing, and written once the version has held still for a moment,
 * or after a while regardless if it never does, each write being one transaction holding only the records that
 * changed. Whatever has not been written is written by stop().
 *
 * @note stop() must be called before the hive is shut down, and before this is released: until then the hive
 *       holds a reference to this as one of its listeners.
 * @note Writes are only ever made from one thread at a time: by flush() before start(), by the writer thread
 *       between start() and stop(), and by stop() itself once that thread has ended. A node joining the hive is
 *       captured on whichever thread added it.
 */
class HivePersister : public RefCounted
{
	public:

		/**
		 * @param store Store the hive is kept in.
		 * @param hive Hive to keep stored.
		 * @param loader Loader the hive was built from, which gives each of its nodes as it was loaded and the
		 *        order its nodes and surfaces go in.
		 * @throw PersistException(LMDB_TXN_FAILED) If what the store already holds of the hive cannot be read.
		 */
		HivePersister(Handle<LmdbHiveStore> store, Handle<GraphHive> hive, HiveLoader& loader);

		/**
		 * @param store Store the hive is kept in.
		 * @param hive Hive to keep stored.
		 * @param recorder Recorder of the hive, shared with whatever else needs it. Started by start(), but left
		 *        for its owner to stop.
		 * @throw PersistException(LMDB_TXN_FAILED) If what the store already holds of the hive cannot be read.
		 */
		HivePersister(Handle<LmdbHiveStore> store, Handle<GraphHive> hive, Handle<HiveRecorder> recorder);

		/**
		 * Start keeping the stored copy up to date: listen for nodes joining the hive, and start the thread that
		 * writes changes.
		 * @note Can only be called once.
		 * @throw ThreadException If the writer thread cannot be started.
		 */
		void start();

		/**
		 * Write whatever about the hive differs from what is stored, now.
		 * @note Must not be called between start() and stop(), when the writer thread is the one writing.
		 * @throw PersistException(LMDB_TXN_FAILED) If the store cannot be written.
		 * @throw PersistException(HIVE_NOT_CAPTURABLE) If the hive holds a node no hive definition can describe.
		 */
		void flush();

		/**
		 * Stop keeping the stored copy up to date, writing whatever has not yet been written first.
		 * @note Safe to call more than once, and without start() having been called. A failure to write is
		 *       reported rather than raised, as there is nothing left to retry it.
		 */
		void stop();

	protected:

		// Ref counted.
		virtual ~HivePersister();

	private:

		/**
		 * Thread that writes a hive's changes once they have settled.
		 */
		class WriterThread : public Thread
		{
			public:

				WriterThread(HivePersister& persister);

				virtual ~WriterThread();

				virtual void threadEntry() override;

			protected:

				virtual void _quitRequested() override;

			private:

				// Disable copying.
				WriterThread(const WriterThread& copyFrom);
				WriterThread& operator= (const WriterThread& copyFrom);

				/// Persister whose hive this writes. Outlives this, as it stops this before being released.
				HivePersister& _persister;

				/// Provides the timed wait between polls, and the wake when a stop is asked for.
				ThreadCondition _cond;
		};

		// Disable copying.
		HivePersister(const HivePersister& copyFrom);
		HivePersister& operator= (const HivePersister& copyFrom);

		/**
		 * Check the hive's version, and write if a change has settled or has been waiting too long.
		 * @note Called by the writer thread on every poll. A failure to write is reported and retried at the next
		 *       poll rather than raised.
		 */
		void __poll();


		/**
		 * Build every record of the hive as it now stands.
		 * @returns Record value by key.
		 */
		std::map<std::string, std::string> __buildRecords();

		/// Store the hive is kept in.
		Handle<LmdbHiveStore> _store;

		/// Hive kept stored.
		Handle<GraphHive> _hive;

		/// Name of the hive, which is also the name it is stored under.
		std::string _hiveName;

		/// Records what the hive would have to be rebuilt from.
		Handle<HiveRecorder> _recorder;

		/// Whether the recorder was made by this, and so is stopped by it too.
		bool _ownsRecorder = false;

		/// Each record as last written, or as found in the store before anything was written, by key. Only
		/// touched by whichever thread is writing.
		std::map<std::string, std::string> _written;

		/// Hive version the stored copy was last brought up to date with. Zero until it first has been. Only
		/// touched by whichever thread is writing.
		unsigned _writtenVersion = 0;

		/// Last hive version the writer thread saw that has not been written. Zero if every version it has seen
		/// has been. Only touched by the writer thread.
		unsigned _pendingVersion = 0;

		/// Monotonic time in ms at which _pendingVersion was first seen. Only touched by the writer thread.
		long long _pendingVersionSeenMs = 0;

		/// Monotonic time in ms at which the oldest change not yet written was first seen. Only touched by the
		/// writer thread.
		long long _firstPendingMs = 0;

		/// Writer thread. Only present between start() and stop().
		WriterThread* _writerThread = 0;

		/// Whether start() has been called.
		bool _started = false;

		/// Whether stop() has been called.
		bool _stopped = false;

		/// Generic lock.
		ThreadMutex _lock;
};

#endif
