#ifndef DISPLAY_EXCEPTION_H
#define DISPLAY_EXCEPTION_H

#include "../util/Exception.hpp"

/**
 * Exception raised by the display module.
 */
class DisplayException : public Exception
{
    public:

        enum Error
        {
            UNKNOWN,
            /// The underlying HTTP server implementation could not be started.
            HTTP_SERVER_START_FAILED,
            /// A handler has already been registered for the given path.
            DUPLICATE_HTTP_HANDLER_PATH
        };

        virtual ~DisplayException(){}

        DisplayException(Error error) : Exception(Exception::DISPLAY), _error{error} {}

        Error getError() {return _error;}

    private:

        Error _error;
};

#endif
