#include "../log/log.hpp"

#include "ThreadException.hpp"
#include "ThreadLock.hpp"

ThreadLock::~ThreadLock()
{
    _mutex -> unlock();
}

ThreadLock::ThreadLock(ThreadMutex* mutex) : _mutex(mutex)
{
	try
	{
		_locked = mutex -> lock();
	}
	catch(const ThreadException ex)
	{
		LOG(Logger::LogLevel::DEBUG, "Could not obtain thread lock.")
	}
}
