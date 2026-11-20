#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <map>
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

        /**
         * Set a header of this request.
         * @param name Name of the header. Matched without regard to case, as HTTP header names are.
         * @param value Value of the header. Replaces any value already set under the same name.
         */
        void setHeader(std::string name, std::string value);

        /**
         * Get a header of this request by name.
         * @param name Name of the header, in any case.
         * @returns Value of the header, or an empty string if not present.
         */
        std::string getHeader(std::string name);

        /**
         * Set the address of the client this request came from.
         * @param remoteAddress Address of the client, without its port, e.g. "192.168.1.4".
         */
        void setRemoteAddress(std::string remoteAddress);

        /**
         * Get the address of the client this request came from.
         * @returns Address of the client, without its port. Empty if it was never set.
         */
        std::string getRemoteAddress();

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

        /// Headers of this request, keyed by lower cased name.
        std::map<std::string, std::string> _headers;

        /// Address of the client this request came from.
        std::string _remoteAddress;
};

#endif
