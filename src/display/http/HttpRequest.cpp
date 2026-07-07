#include "HttpRequest.hpp"

#include <stdlib.h>

namespace
{
	// Decodes a URL encoded string, e.g. "a+b%2Fc" -> "a b/c".
	std::string urlDecode(const std::string& value)
	{
		std::string decoded;

		for(std::size_t index = 0; index < value.size(); index++)
		{
			char currentChar = value[index];

			if(currentChar == '+')
			{
				decoded += ' ';
			}
			else if(currentChar == '%' && index + 2 < value.size())
			{
				std::string hex = value.substr(index + 1, 2);

				decoded += (char) strtol(hex.c_str(), 0, 16);

				index += 2;
			}
			else
			{
				decoded += currentChar;
			}
		}

		return decoded;
	}
}

HttpRequest::HttpRequest(std::string method, std::string path, std::string query, std::string body) :
	_method{method}, _path{path}, _query{query}, _body{body}
{
}

std::string HttpRequest::getMethod()
{
	return _method;
}

std::string HttpRequest::getPath()
{
	return _path;
}

std::string HttpRequest::getQuery()
{
	return _query;
}

std::string HttpRequest::getBody()
{
	return _body;
}

std::string HttpRequest::getQueryParam(std::string name)
{
	std::size_t searchStart = 0;

	while(searchStart < _query.size())
	{
		std::size_t ampersandPos = _query.find('&', searchStart);

		std::string entry = _query.substr(searchStart, ampersandPos == std::string::npos ?
			std::string::npos : ampersandPos - searchStart);

		std::size_t equalsPos = entry.find('=');

		std::string entryName = (equalsPos == std::string::npos) ? entry : entry.substr(0, equalsPos);

		if(entryName == name)
		{
			return (equalsPos == std::string::npos) ? "" : urlDecode(entry.substr(equalsPos + 1));
		}

		if(ampersandPos == std::string::npos) break;

		searchStart = ampersandPos + 1;
	}

	return "";
}
