#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "ThreadPthread.hpp"
#include "ThreadException.hpp"

ThreadPthread::~ThreadPthread()
{
}

ThreadPthread::ThreadPthread()
{
}

bool ThreadPthread::_start(void*(*threadEntry)(void*))
{
   int error = pthread_create(&_thread, NULL, threadEntry, (void*) this);

	switch(error)
	{
		case EAGAIN:

			throw ThreadException(ThreadException::THREAD_RESOURCES_EXHAUSTED, error,
				"EAGAIN The system lacked the necessary resources to create another thread.");
			break;

		case EINVAL:

			throw ThreadException(ThreadException::THREAD_INCORRECT_PARAMETER, error,
				"EINVAL The value specified by attr is invalid.");
			break;

		case EPERM:

			throw ThreadException(ThreadException::THREAD_INSUFFICIENT_PRIVILEGES, error,
				"EPERM The caller does not have appropriate permission.");
			break;

		default:
			if(error) throw ThreadException(ThreadException::THREAD_GENERAL_ERROR, error);
			break;
	}

	return true;
}

void ThreadPthread::_forceStop()
{
	int error = pthread_cancel(_thread);

	if(error)
	{
		switch(error)
		{
			case ESRCH:

				throw ThreadException(ThreadException::THREAD_INCORRECT_PARAMETER, error,
					"ESRCH Thread ID could not be found.");
				break;

			default:

				throw ThreadException(ThreadException::THREAD_GENERAL_ERROR, error);
				break;
		}
	}
}

