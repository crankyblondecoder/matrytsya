/** Copyright Scott A.E. Lanham, Australia */

#ifndef THREAD_EXCEPTION_H
#define THREAD_EXCEPTION_H

#include <string>

#include "../util/Exception.hpp"

/**
 * Create and invoke thread entry point.
 */
class ThreadException : public Exception
{
    public:

        enum Error
        {
            UNKNOWN,
            THREAD_ALREADY_STARTED,
            THREAD_RESOURCES_EXHAUSTED,
            THREAD_INCORRECT_PARAMETER,
            THREAD_INSUFFICIENT_PRIVILEGES,
			THREAD_SLEEP_INTERRUPTED,
            THREAD_GENERAL_ERROR,
            MUTEX_RESOURCES_EXHAUSTED,
            MUTEX_INSUFFICIENT_PRIVILEGES,
            MUTEX_PRIORITY_CONFLICT,
            MUTEX_INCORRECT_PARAMETER,
            MUTEX_BUSY,
            MUTEX_GENERAL_ERROR,
            CONDITION_RESOURCES_EXHAUSTED,
            CONDITION_BUSY,
            CONDITION_INCORRECT_PARAMETER,
            CONDITION_MUTEX_ERROR,
            CONDITION_INSUFFICIENT_PRIVILEGES,
            CONDITION_GENERAL_ERROR,
			/// General thread pool out of memory.
			THREAD_POOL_BAD_ALLOC,
			/// Could not allocate thread pool worker.
			THREAD_POOL_CANT_ALLOC_WORKER,
			/// Could not start any thread pool workers.
			THREAD_POOL_COULD_NOT_START_ANY_WORKERS
        };

        virtual ~ThreadException(){}

        ThreadException(Error error, int subsystemErrorCode = 0, const char* subsystemErrorDescr = "") :
			Exception(Exception::THREAD),
            _error{error}, _subsystemErrorCode{subsystemErrorCode},
            _subsystemErrorDescr{subsystemErrorDescr} {}

        Error getError() {return _error;}

		std::string getSubsystemErrorString() {return _subsystemErrorDescr;}

    private:

        /** Non-subsystem specific error. */
        Error _error;

        /** Subsystem specific error code. For example pthread error code. Set to 0 as default. */
        int _subsystemErrorCode;

        /** Subsystem specific error description that relates to error code. */
        std::string _subsystemErrorDescr;
};

#endif