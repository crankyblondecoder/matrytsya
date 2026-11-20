#ifndef LMDB_HIVE_STORE_H
#define LMDB_HIVE_STORE_H

#include <map>
#include <string>
#include <vector>

#include "../../thread/ThreadMutex.hpp"
#include "../../util/RefCounted.hpp"

struct MDB_env;

/**
 * An LMDB database file holding any number of hives, each as a set of string keyed records in a named database
 * of its own, named after the hive. What the records hold is up to the caller; see lmdbHiveRecords.hpp for the
 * layout hives are stored in.
 * @note The database is a single file at the path given, with LMDB's lock file beside it at that path plus
 *       "-lock". Both are created if they do not exist. The lock file is removed when the store is destroyed,
 *       unless another process still has the database open.
 * @note Thread safe. Each read or write is a transaction of its own, and transactions are run one at a time.
 */
class LmdbHiveStore : public RefCounted
{
	public:

		/**
		 * Open a database file, creating it if it does not exist.
		 * @param path Path of the database file.
		 * @throw PersistException(LMDB_OPEN_FAILED) If the file cannot be opened or created.
		 */
		LmdbHiveStore(const std::string& path);

		/**
		 * Get the path of the database file.
		 */
		const std::string& getPath();

		/**
		 * Get the names of every hive held.
		 * @returns Hive names, in byte order.
		 * @throw PersistException(LMDB_TXN_FAILED) If the database cannot be read.
		 */
		std::vector<std::string> getHiveNames();

		/**
		 * Get whether a hive is held.
		 * @param hiveName Name of the hive.
		 * @returns True if a hive of that name is held.
		 * @throw PersistException(LMDB_TXN_FAILED) If the database cannot be read.
		 */
		bool hasHive(const std::string& hiveName);

		/**
		 * Read every record of a hive.
		 * @param hiveName Name of the hive.
		 * @returns Record value by key. Empty if no hive of that name is held.
		 * @throw PersistException(LMDB_TXN_FAILED) If the database cannot be read.
		 */
		std::map<std::string, std::string> readRecords(const std::string& hiveName);

		/**
		 * Write records of a hive in one transaction, creating the hive if it is not already held. Either every
		 * change is made or, on failure, none is.
		 * @param hiveName Name of the hive.
		 * @param puts Records to write, by key. Each replaces any record already held under its key.
		 * @param deletes Keys of records to remove. A key with no record is passed over.
		 * @throw PersistException(LMDB_TXN_FAILED) If the database cannot be written.
		 */
		void writeRecords(const std::string& hiveName, const std::map<std::string, std::string>& puts,
			const std::vector<std::string>& deletes);

		/**
		 * Remove a hive, and every record of it, from the store.
		 * @param hiveName Name of the hive.
		 * @returns True if the hive was held and has been removed. False if no hive of that name was held.
		 * @throw PersistException(LMDB_TXN_FAILED) If the database cannot be written.
		 */
		bool deleteHive(const std::string& hiveName);

	protected:

		// Ref counted.
		virtual ~LmdbHiveStore();

	private:

		// Disable copying.
		LmdbHiveStore(const LmdbHiveStore& copyFrom);
		LmdbHiveStore& operator= (const LmdbHiveStore& copyFrom);

		/// Path of the database file.
		std::string _path;

		/// LMDB environment of the open database file.
		MDB_env* _env = 0;

		/// Serialises transactions. LMDB requires a transaction that opens a named database to finish before any
		/// other in the process opens one, and every transaction here may.
		ThreadMutex _txnLock;
};

#endif
