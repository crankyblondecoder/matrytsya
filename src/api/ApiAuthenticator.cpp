#include "ApiAuthenticator.hpp"

#include "ApiException.hpp"
#include "../display/http/HttpRequest.hpp"
#include "../thread/thread.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <time.h>

namespace
{
	const char* const _BEARER_PREFIX = "Bearer ";

	std::string trim(const std::string& value)
	{
		std::size_t start = value.find_first_not_of(" \t\r\n");

		if(start == std::string::npos) return "";

		std::size_t end = value.find_last_not_of(" \t\r\n");

		return value.substr(start, end - start + 1);
	}

	// Overwrites a string's characters, so that a token does not linger in memory once it is no longer needed.
	void wipe(std::string& value)
	{
		volatile char* characters = (volatile char*) value.data();

		for(std::size_t index = 0; index < value.size(); index++) characters[index] = 0;

		value.clear();
	}
}

ApiAuthenticator::ApiAuthenticator(std::string& tokens)
{
	std::size_t start = 0;

	while(start <= tokens.size())
	{
		std::size_t comma = tokens.find(',', start);

		std::string token = trim(tokens.substr(start, comma == std::string::npos ? std::string::npos : comma - start));

		if(!token.empty())
		{
			if(token.size() < MIN_TOKEN_LENGTH)
			{
				wipe(token);
				wipe(tokens);

				throw ApiException(ApiException::MISSING_API_TOKEN,
					"Every API token must be at least " + std::to_string(MIN_TOKEN_LENGTH) + " characters long.");
			}

			_digests.push_back(__digest(token));
		}

		wipe(token);

		if(comma == std::string::npos) break;

		start = comma + 1;
	}

	wipe(tokens);

	if(_digests.empty()) throw ApiException(ApiException::MISSING_API_TOKEN, "No API token was given.");
}

ApiAuthenticator::~ApiAuthenticator()
{
}

ApiAuthenticator::Result ApiAuthenticator::authenticate(HttpRequest& request)
{
	std::string address = request.getRemoteAddress();
	long long now = _nowSeconds();

	{ SYNC(_lock)

		if(__isLimited(address, now)) return RATE_LIMITED;
	}

	std::string authorization = request.getHeader("Authorization");

	bool accepted = false;

	if(authorization.compare(0, std::string(_BEARER_PREFIX).size(), _BEARER_PREFIX) == 0)
	{
		std::string token = trim(authorization.substr(std::string(_BEARER_PREFIX).size()));

		accepted = !token.empty() && __accepts(token);

		wipe(token);
	}

	wipe(authorization);

	{ SYNC(_lock)

		if(accepted)
		{
			_failures.erase(address);
		}
		else
		{
			FailureCount& count = _failures[address];

			if(count.failures == 0) count.windowStart = now;

			count.failures++;
		}
	}

	return accepted ? ACCEPTED : REJECTED;
}

long long ApiAuthenticator::_nowSeconds()
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);

	return (long long) now.tv_sec;
}

std::string ApiAuthenticator::__digest(const std::string& token)
{
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digestLength = 0;

	if(!EVP_Digest(token.data(), token.size(), digest, &digestLength, EVP_sha256(), 0))
	{
		throw ApiException(ApiException::UNKNOWN, "Could not hash an API token.");
	}

	return std::string((const char*) digest, digestLength);
}

bool ApiAuthenticator::__accepts(const std::string& token)
{
	std::string presented = __digest(token);

	int matched = 0;

	// Every digest is compared, whether or not one has matched already, so that how long this takes says nothing
	// about which token, if any, was presented.
	for(const std::string& digest : _digests)
	{
		matched |= (digest.size() == presented.size() &&
			CRYPTO_memcmp(digest.data(), presented.data(), digest.size()) == 0) ? 1 : 0;
	}

	return matched != 0;
}

bool ApiAuthenticator::__isLimited(const std::string& address, long long now)
{
	// Expired counts are pruned on every call, so the table cannot be grown without bound by requests from ever
	// new addresses.
	for(std::map<std::string, FailureCount>::iterator iter = _failures.begin(); iter != _failures.end();)
	{
		if(now - iter -> second.windowStart >= FAILURE_WINDOW_SECONDS) iter = _failures.erase(iter);
		else iter++;
	}

	std::map<std::string, FailureCount>::iterator found = _failures.find(address);

	return found != _failures.end() && found -> second.failures >= MAX_FAILURES;
}
