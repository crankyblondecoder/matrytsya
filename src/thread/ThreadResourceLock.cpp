#include "ThreadResourceLock.hpp"

#include "ThreadException.hpp"

std::atomic<unsigned> ThreadResourceLock::_nextId{1};

ThreadResourceLock::~ThreadResourceLock()
{
}

ThreadResourceLock::ThreadResourceLock() : _id(_nextId++)
{
}

void ThreadResourceLock::lock()
{
	_condition.lockMutex();

	/// Can't already be locked or waiting on a lock.

	try
	{
		if(__hasLock() || __waitingOnLock())
		{
			throw ThreadException(ThreadException::RESOURCE_LOCK_RE_ENTRY);
		}
	}
	catch(ThreadException& ex)
	{
		_condition.unlockMutex();
		throw;
	}

	try
	{
		if(_locked)
		{
			__waitOnLock();
			while(_locked) _condition.wait();
		}
	}
	catch(ThreadException& ex)
	{
		__stopWaitingOnLock();
		_condition.unlockMutex();
		throw;
	}

	try
	{
		__claimLock();
	}
	catch(ThreadException& ex)
	{
		// There still might be other threads blocking.
		_condition.signal();
		_condition.unlockMutex();
		throw;
	}

	_condition.unlockMutex();
}

void ThreadResourceLock::unlock()
{
	_condition.lockMutex();

	try
	{
		if(!__hasLock() || __waitingOnLock())
		{
			throw ThreadException(ThreadException::RESOURCE_LOCK_NO_LOCK);
		}
	}
	catch(ThreadException& ex)
	{
		_condition.unlockMutex();
		throw;
	}

	try
	{
		__releaseLock();
		_condition.signal();
	}
	catch(ThreadException& ex)
	{
		_condition.unlockMutex();
		throw;
	}

	_condition.unlockMutex();
}

bool ThreadResourceLock::hasLock()
{
	return __hasLock();
}

ThreadResourceLock::LockRecord& ThreadResourceLock::__getLockRecord()
{
	for(LockRecord& record : _lockRecords)
	{
		if(record.resourceLockId == _id) return record;
	}

	for(LockRecord& record : _lockRecords)
	{
		if(record.resourceLockId == 0)
		{
			record.resourceLockId = _id;
			record.waiting = false;
			record.hasLock = false;

			return record;
		}
	}

	// Create a new lock record or re-use a nullified existing one.
	try
	{
		_lockRecords.push_back({.resourceLockId = _id, .waiting = false, .hasLock = false});
	}
	catch(...)
	{
		throw ThreadException(ThreadException::RESOURCE_LOCK_ERROR);
	}

	return _lockRecords.back();
}

void ThreadResourceLock::__waitOnLock()
{
	LockRecord& record = __getLockRecord();

	record.waiting = true;
	record.hasLock = false;
}

void ThreadResourceLock::__stopWaitingOnLock()
{
	for(LockRecord& record : _lockRecords)
	{
		if(record.resourceLockId == _id) record.resourceLockId = 0;
	}
}

bool ThreadResourceLock::__waitingOnLock()
{
	bool retVal = false;

	for(LockRecord& record : _lockRecords)
	{
		if(record.resourceLockId == _id && record.waiting) retVal = true;
	}

	return retVal;
}

void ThreadResourceLock::__claimLock()
{
	_locked = true;

	try
	{
		LockRecord& record = __getLockRecord();
		record.hasLock = true;
		record.waiting = false;
	}
	catch(...)
	{
		_locked = false;
		throw;
	}
}

bool ThreadResourceLock::__hasLock()
{
	bool retVal = false;

	for(LockRecord& record : _lockRecords)
	{
		if(record.resourceLockId == _id && record.hasLock) retVal = true;
	}

	return retVal;
}

void ThreadResourceLock::__releaseLock()
{
	_locked = false;

	for(LockRecord& record : _lockRecords)
	{
		if(record.resourceLockId == _id) record.resourceLockId = 0;
	}
}

