#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <utility>
#include <vector>

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

        /**
         * Add a header to this response, beyond the content type, which is always sent.
         * @param name Name of the header.
         * @param value Value of the header.
         * @note A name or value holding a carriage return or line feed is dropped, as it would otherwise let
         *       whatever set it write further headers of its own.
         */
        void addHeader(std::string name, std::string value);

        /**
         * Get the headers added to this response, in the order they were added.
         */
        std::vector<std::pair<std::string, std::string>> getHeaders();

    protected:

    private:

        /// HTTP status code of this response.
        unsigned _statusCode = 200;

        /// Content type of this response.
        std::string _contentType = "text/plain";

        /// Body of this response.
        std::string _body;

        /// Headers added to this response, beyond the content type.
        std::vector<std::pair<std::string, std::string>> _headers;
};

#endif
