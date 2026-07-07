#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>

/**
 * Represents an incoming HTTP request, independent of the underlying HTTP server implementation.
 */
class HttpRequest
{
    public:

        HttpRequest(std::string method, std::string path, std::string query, std::string body);

        /**
         * Get the HTTP method of this request, e.g. "GET".
         */
        std::string getMethod();

        /**
         * Get the path component of this request, e.g. "/foo/bar".
         */
        std::string getPath();

        /**
         * Get the raw, still URL encoded, query string of this request, e.g. "a=1&b=2".
         */
        std::string getQuery();

        /**
         * Get the body of this request.
         */
        std::string getBody();

        /**
         * Get a URL decoded query parameter value by name.
         * @param name Name of the parameter to look up.
         * @returns Decoded value of the parameter, or an empty string if not present.
         */
        std::string getQueryParam(std::string name);

    protected:

    private:

        /// HTTP method of this request, e.g. "GET".
        std::string _method;

        /// Path component of this request.
        std::string _path;

        /// Raw query string of this request.
        std::string _query;

        /// Body of this request.
        std::string _body;
};

#endif
