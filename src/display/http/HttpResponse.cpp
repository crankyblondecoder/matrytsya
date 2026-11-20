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

void HttpResponse::addHeader(std::string name, std::string value)
{
	if(name.find_first_of("\r\n") != std::string::npos || value.find_first_of("\r\n") != std::string::npos) return;

	_headers.push_back(std::make_pair(name, value));
}

std::vector<std::pair<std::string, std::string>> HttpResponse::getHeaders()
{
	return _headers;
}
