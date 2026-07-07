#include "HttpResponse.hpp"

HttpResponse::HttpResponse()
{
}

void HttpResponse::setStatus(unsigned statusCode)
{
	_statusCode = statusCode;
}

unsigned HttpResponse::getStatus()
{
	return _statusCode;
}

void HttpResponse::setContentType(std::string contentType)
{
	_contentType = contentType;
}

std::string HttpResponse::getContentType()
{
	return _contentType;
}

void HttpResponse::setBody(std::string body)
{
	_body = body;
}

std::string HttpResponse::getBody()
{
	return _body;
}
