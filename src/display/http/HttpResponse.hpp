#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>

/**
 * Represents an outgoing HTTP response, independent of the underlying HTTP server implementation.
 */
class HttpResponse
{
    public:

        HttpResponse();

        /**
         * Set the HTTP status code of this response, e.g. 200.
         */
        void setStatus(unsigned statusCode);

        unsigned getStatus();

        /**
         * Set the content type of this response, e.g. "text/html".
         */
        void setContentType(std::string contentType);

        std::string getContentType();

        /**
         * Set the body of this response.
         */
        void setBody(std::string body);

        std::string getBody();

    protected:

    private:

        /// HTTP status code of this response.
        unsigned _statusCode = 200;

        /// Content type of this response.
        std::string _contentType = "text/plain";

        /// Body of this response.
        std::string _body;
};

#endif
