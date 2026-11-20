#ifndef API_AUTHENTICATOR_UNIT_TEST_H
#define API_AUTHENTICATOR_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../api/ApiAuthenticator.hpp"
#include "../../api/ApiException.hpp"
#include "../../display/http/HttpRequest.hpp"

#include <string>

namespace
{
	const std::string _AUTH_TEST_TOKEN = "0123456789abcdef0123456789abcdef";
	const std::string _AUTH_TEST_OTHER_TOKEN = "fedcba9876543210fedcba9876543210";

	/**
	 * An authenticator whose clock the test sets.
	 */
	class ClockedAuthenticator : public ApiAuthenticator
	{
		public:

			ClockedAuthenticator(std::string& tokens) : ApiAuthenticator(tokens) {}

			long long now = 1000;

		protected:

			long long _nowSeconds() override { return now; }
	};

	ApiAuthenticator::Result _authenticate(ApiAuthenticator& authenticator, const std::string& authorization,
		const std::string& address = "10.0.0.1")
	{
		HttpRequest request("GET", "/api/v1/hives", "", "");

		if(!authorization.empty()) request.setHeader("Authorization", authorization);

		request.setRemoteAddress(address);

		return authenticator.authenticate(request);
	}
}

/**
 * Only a request carrying an accepted token as a bearer token is accepted, and the token string handed in is wiped.
 */
TEST(ApiAuthenticatorTest, AcceptsOnlyTheBearerToken)
{
	std::string tokens = _AUTH_TEST_TOKEN;

	ApiAuthenticator authenticator(tokens);

	EXPECT_TRUE(tokens.empty()) << "The tokens handed in should be wiped once read.";

	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_TOKEN), ApiAuthenticator::ACCEPTED);
	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_OTHER_TOKEN), ApiAuthenticator::REJECTED);
	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_TOKEN.substr(1)), ApiAuthenticator::REJECTED);
	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_TOKEN + "x"), ApiAuthenticator::REJECTED);
	EXPECT_EQ(_authenticate(authenticator, ""), ApiAuthenticator::REJECTED);
	EXPECT_EQ(_authenticate(authenticator, "Basic " + _AUTH_TEST_TOKEN), ApiAuthenticator::REJECTED);
	EXPECT_EQ(_authenticate(authenticator, _AUTH_TEST_TOKEN), ApiAuthenticator::REJECTED);
	EXPECT_EQ(_authenticate(authenticator, "Bearer "), ApiAuthenticator::REJECTED);
}

/**
 * Several tokens can be accepted at once, so a token can be rotated.
 */
TEST(ApiAuthenticatorTest, AcceptsEachOfSeveralTokens)
{
	std::string tokens = " " + _AUTH_TEST_TOKEN + " , " + _AUTH_TEST_OTHER_TOKEN + ",";

	ApiAuthenticator authenticator(tokens);

	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_TOKEN), ApiAuthenticator::ACCEPTED);
	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_OTHER_TOKEN), ApiAuthenticator::ACCEPTED);
}

/**
 * No token, or one too short to be safe, is refused outright.
 */
TEST(ApiAuthenticatorTest, RefusesMissingOrShortTokens)
{
	std::string empty = " , ";

	EXPECT_THROW({ ApiAuthenticator authenticator(empty); }, ApiException);

	std::string shortToken = _AUTH_TEST_TOKEN.substr(0, ApiAuthenticator::MIN_TOKEN_LENGTH - 1);

	try
	{
		ApiAuthenticator authenticator(shortToken);

		FAIL() << "A token shorter than the minimum should be refused.";
	}
	catch(ApiException& exception)
	{
		EXPECT_EQ(exception.getError(), ApiException::MISSING_API_TOKEN);
	}

	std::string oneShort = _AUTH_TEST_TOKEN + ",short";

	EXPECT_THROW({ ApiAuthenticator authenticator(oneShort); }, ApiException);
}

/**
 * An address that fails too often is refused, even with a valid token, until the window has passed, while other
 * addresses are unaffected. A success clears the count.
 */
TEST(ApiAuthenticatorTest, RateLimitsFailingAddresses)
{
	std::string tokens = _AUTH_TEST_TOKEN;

	ClockedAuthenticator authenticator(tokens);

	for(unsigned attempt = 0; attempt < ApiAuthenticator::MAX_FAILURES - 1; attempt++)
	{
		EXPECT_EQ(_authenticate(authenticator, "Bearer wrong"), ApiAuthenticator::REJECTED);
	}

	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_TOKEN), ApiAuthenticator::ACCEPTED)
		<< "One failure short of the limit should not lock the address out.";

	for(unsigned attempt = 0; attempt < ApiAuthenticator::MAX_FAILURES; attempt++)
	{
		EXPECT_EQ(_authenticate(authenticator, "Bearer wrong"), ApiAuthenticator::REJECTED)
			<< "The success above should have cleared the count.";
	}

	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_TOKEN), ApiAuthenticator::RATE_LIMITED);
	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_TOKEN, "10.0.0.2"), ApiAuthenticator::ACCEPTED)
		<< "Another address should be unaffected.";

	authenticator.now += ApiAuthenticator::FAILURE_WINDOW_SECONDS - 1;

	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_TOKEN), ApiAuthenticator::RATE_LIMITED);

	authenticator.now += 1;

	EXPECT_EQ(_authenticate(authenticator, "Bearer " + _AUTH_TEST_TOKEN), ApiAuthenticator::ACCEPTED)
		<< "The lockout should end with the window.";
}

#endif
