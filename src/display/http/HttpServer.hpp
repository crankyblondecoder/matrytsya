#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "HttpServerMongoose.hpp"

// TODO Support for swapping out mongoose for another embedded HTTP transport, if ever needed.
typedef HttpServerMongoose HttpServer;

#endif
