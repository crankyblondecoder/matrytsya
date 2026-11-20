#include "LmdbHiveStore.hpp"

#include "../PersistException.hpp"
#include "../../thread/thread.hpp"

#include <fcntl.h>
#include <lmdb.h>
#include <unistd.h>

namespace
{
	/// Upper bound on how large the database file may grow. Only address space is reserved up front; the file
	/// itself grows as records are written.
	const size_t _MAP_SIZE = (size_t) 1 << 30;

	/// Upper bound on the number of hives a database can hold, as each is a named database of its own.
	const unsigned _MAX_HIVES = 256;

	/// Read and write for owner and group, read for others, as a newly created file is given.
	const mdb_mode_t _FILE_MODE = 0664;

	/**
	 * Describe an LMDB failure.
	 * @param operation What was being done.
	 * @param rc Return code it failed with.
	 */
	std::string __describe(const std::string& operation, int rc)
	{
		return operation + ": " + mdb_strerror(rc);
	}

	/**
	 * Remove an LMDB lock file, unless some other process still has the database open.
	 * @param lockPath Path of the lock file.
	 * @note LMDB never removes its lock file itself, not even on a clean close.
	 * @note Call only after this process has closed its own environment on the database. Closing it releases
	 *       this process's locks on the file, which would otherwise hide whether any other process holds one.
	 */
	void __removeLockFileIfUnused(const std::string& lockPath)
	{
		int fd = open(lockPath.c_str(), O_RDWR);

		if(fd < 0) return;

		// Every process with the database open holds a shared lock on the first byte of the lock file for as
		// long as it does, so an exclusive lock on that byte can only be had once no process is using it.
		// A process that opened the file just before the unlink waits on this lock and then carries on with
		// the unlinked file, which is only unsafe if a third opens the database while it still runs.
		struct flock lock = {};
		lock.l_type = F_WRLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = 0;
		lock.l_len = 1;

		if(fcntl(fd, F_SETLK, &lock) == 0) unlink(lockPath.c_str());

		close(fd);
	}

	MDB_val __toVal(const std::string& text)
	{
		MDB_val value;

		value.mv_size = text.size();
		value.mv_data = (void*) text.data();

		return value;
	}

	std::string __fromVal(const MDB_val& value)
	{
		return std::string((const char*) value.mv_data, value.mv_size);
	}

	/**
	 * Abort a transaction when it goes out of scope without having been committed, so that every path out of a
	 * read or write releases it.
	 */
	class TxnGuard
	{
		public:

			TxnGuard(MDB_txn* txn) : _txn{txn} {}

			~TxnGuard()
			{
				if(_txn) mdb_txn_abort(_txn);
			}

			/**
			 * Commit the transaction.
			 * @returns LMDB return code of the commit.
			 */
			int commit()
			{
				// A commit frees the transaction whether or not it succeeds, so it must not be aborted after.
				MDB_txn* txn = _txn;
				_txn = 0;

				return mdb_txn_commit(txn);
			}

		private:

			MDB_txn* _txn;
	};
}

LmdbHiveStore::LmdbHiveStore(const std::string& path) : _path{path}
{
	int rc = mdb_env_create(&_env);

	if(rc != MDB_SUCCESS)
	{
		_env = 0;
		throw PersistException(PersistException::LMDB_OPEN_FAILED, __describe(path, rc));
	}

	rc = mdb_env_set_mapsize(_env, _MAP_SIZE);

	if(rc == MDB_SUCCESS) rc = mdb_env_set_maxdbs(_env, _MAX_HIVES);

	// No subdirectory so that the path names the database file itself, as the command line takes it. No
	// thread local storage as transactions are begun on whichever thread happens to need one.
	if(rc == MDB_SUCCESS) rc = mdb_env_open(_env, path.c_str(), MDB_NOSUBDIR | MDB_NOTLS, _FILE_MODE);

	if(rc != MDB_SUCCESS)
	{
		mdb_env_close(_env);
		_env = 0;

		throw PersistException(PersistException::LMDB_OPEN_FAILED, __describe(path, rc));
	}
}

LmdbHiveStore::~LmdbHiveStore()
{
	if(!_env) return;

	mdb_env_close(_env);

	__removeLockFileIfUnused(_path + "-lock");
}

const std::string& LmdbHiveStore::getPath()
{
	return _path;
}

std::vector<std::string> LmdbHiveStore::getHiveNames()
{
	std::vector<std::string> names;

	// LMDB calls are made under this lock, which no LMDB call can re-enter.
	{ SYNC(_txnLock)

		MDB_txn* txn = 0;

		int rc = mdb_txn_begin(_env, 0, MDB_RDONLY, &txn);

		if(rc != MDB_SUCCESS) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("begin read", rc));

		TxnGuard guard(txn);

		// The unnamed database holds nothing but the name of each named database, which is a hive.
		MDB_dbi mainDbi;

		rc = mdb_dbi_open(txn, 0, 0, &mainDbi);

		if(rc != MDB_SUCCESS) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("open main", rc));

		MDB_cursor* cursor = 0;

		rc = mdb_cursor_open(txn, mainDbi, &cursor);

		if(rc != MDB_SUCCESS) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("open cursor", rc));

		MDB_val key;
		MDB_val value;

		while((rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT)) == MDB_SUCCESS)
		{
			names.push_back(__fromVal(key));
		}

		mdb_cursor_close(cursor);

		if(rc != MDB_NOTFOUND) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("list hives", rc));
	}

	return names;
}

bool LmdbHiveStore::hasHive(const std::string& hiveName)
{
	for(const std::string& name : getHiveNames())
	{
		if(name == hiveName) return true;
	}

	return false;
}

std::map<std::string, std::string> LmdbHiveStore::readRecords(const std::string& hiveName)
{
	std::map<std::string, std::string> records;

	// LMDB calls are made under this lock, which no LMDB call can re-enter.
	{ SYNC(_txnLock)

		MDB_txn* txn = 0;

		int rc = mdb_txn_begin(_env, 0, MDB_RDONLY, &txn);

		if(rc != MDB_SUCCESS) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("begin read", rc));

		TxnGuard guard(txn);

		MDB_dbi dbi;

		rc = mdb_dbi_open(txn, hiveName.c_str(), 0, &dbi);

		if(rc == MDB_NOTFOUND) return records;

		if(rc != MDB_SUCCESS)
		{
			throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("open hive '" + hiveName + "'", rc));
		}

		MDB_cursor* cursor = 0;

		rc = mdb_cursor_open(txn, dbi, &cursor);

		if(rc != MDB_SUCCESS) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("open cursor", rc));

		MDB_val key;
		MDB_val value;

		while((rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT)) == MDB_SUCCESS)
		{
			records.emplace(__fromVal(key), __fromVal(value));
		}

		mdb_cursor_close(cursor);

		if(rc != MDB_NOTFOUND)
		{
			throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("read hive '" + hiveName + "'", rc));
		}
	}

	return records;
}

void LmdbHiveStore::writeRecords(const std::string& hiveName, const std::map<std::string, std::string>& puts,
	const std::vector<std::string>& deletes)
{
	// LMDB calls are made under this lock, which no LMDB call can re-enter.
	{ SYNC(_txnLock)

		MDB_txn* txn = 0;

		int rc = mdb_txn_begin(_env, 0, 0, &txn);

		if(rc != MDB_SUCCESS) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("begin write", rc));

		TxnGuard guard(txn);

		MDB_dbi dbi;

		rc = mdb_dbi_open(txn, hiveName.c_str(), MDB_CREATE, &dbi);

		if(rc != MDB_SUCCESS)
		{
			throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("open hive '" + hiveName + "'", rc));
		}

		for(const std::string& key : deletes)
		{
			MDB_val keyVal = __toVal(key);

			rc = mdb_del(txn, dbi, &keyVal, 0);

			if(rc != MDB_SUCCESS && rc != MDB_NOTFOUND)
			{
				throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("delete '" + key + "'", rc));
			}
		}

		for(const std::pair<const std::string, std::string>& record : puts)
		{
			MDB_val keyVal = __toVal(record.first);
			MDB_val valueVal = __toVal(record.second);

			rc = mdb_put(txn, dbi, &keyVal, &valueVal, 0);

			if(rc != MDB_SUCCESS)
			{
				throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("write '" + record.first + "'", rc));
			}
		}

		rc = guard.commit();

		if(rc != MDB_SUCCESS) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("commit", rc));
	}
}

bool LmdbHiveStore::deleteHive(const std::string& hiveName)
{
	// LMDB calls are made under this lock, which no LMDB call can re-enter.
	{ SYNC(_txnLock)

		MDB_txn* txn = 0;

		int rc = mdb_txn_begin(_env, 0, 0, &txn);

		if(rc != MDB_SUCCESS) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("begin write", rc));

		TxnGuard guard(txn);

		MDB_dbi dbi;

		// Opened without MDB_CREATE, so that removing a hive that was never stored creates nothing.
		rc = mdb_dbi_open(txn, hiveName.c_str(), 0, &dbi);

		if(rc == MDB_NOTFOUND) return false;

		if(rc != MDB_SUCCESS)
		{
			throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("open hive '" + hiveName + "'", rc));
		}

		// Deletes the hive's named database as well as emptying it, so that it no longer lists as a hive.
		rc = mdb_drop(txn, dbi, 1);

		if(rc != MDB_SUCCESS)
		{
			throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("drop hive '" + hiveName + "'", rc));
		}

		rc = guard.commit();

		if(rc != MDB_SUCCESS) throw PersistException(PersistException::LMDB_TXN_FAILED, __describe("commit", rc));
	}

	return true;
}
