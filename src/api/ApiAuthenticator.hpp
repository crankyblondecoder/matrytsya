#ifndef API_AUTHENTICATOR_H
#define API_AUTHENTICATOR_H

#include <map>
#include <string>
#include <vector>

#include "../thread/ThreadMutex.hpp"

class HttpRequest;

/**
 * Decides whether a request to the control API carries a valid API token.
 *
 * A request authenticates by carrying "Authorization: Bearer <token>". Only a SHA-256 digest of each accepted
 * token is kept, and a presented token is compared against every one of them in constant time, so neither the
 * memory of the process nor the time taken to answer gives away a token.
 *
 * An address whose requests fail to authenticate too often within a window is refused outright, valid token or
 * not, until that window has passed.
 * @note Thread safe.
 */
class ApiAuthenticator
{
	public:

		/**
		 * Outcome of authenticating a request.
		 */
		enum Result
		{
			/// The request carried an accepted token.
			ACCEPTED,
			/// The request carried no token, or one that is not accepted.
			REJECTED,
			/// The request's address has failed too often of late, so the request was not considered.
			RATE_LIMITED
		};

		/// Fewest characters an accepted token may have.
		static const unsigned MIN_TOKEN_LENGTH = 32;

		/// Failed attempts from one address within a window after which the address is refused.
		static const unsigned MAX_FAILURES = 10;

		/// Length of the window failures are counted over, in seconds.
		static const unsigned FAILURE_WINDOW_SECONDS = 60;

		/**
		 * @param tokens Accepted tokens, separated by commas so that a new token can be accepted alongside the
		 *        old while clients move over to it. Whitespace around each is ignored. Overwritten once read.
		 * @throw ApiException(MISSING_API_TOKEN) If there is no token, or any is shorter than MIN_TOKEN_LENGTH.
		 */
		ApiAuthenticator(std::string& tokens);

		virtual ~ApiAuthenticator();

		/**
		 * Authenticate a request.
		 * @param request Request to authenticate.
		 * @returns Whether it is accepted, rejected, or refused for its address having failed too often.
		 */
		Result authenticate(HttpRequest& request);

	protected:

		/**
		 * Get the current time in seconds, from a clock that only ever goes forward.
		 * @note Overridable so that the failure window can be tested without waiting it out.
		 */
		virtual long long _nowSeconds();

	private:

		// Disable copying.
		ApiAuthenticator(const ApiAuthenticator& copyFrom);
		ApiAuthenticator& operator= (const ApiAuthenticator& copyFrom);

		/**
		 * Failed attempts made from one address.
		 */
		struct FailureCount
		{
			/// Time, in seconds, the window being counted began.
			long long windowStart = 0;

			/// Failures within the window.
			unsigned failures = 0;
		};

		/**
		 * Get the SHA-256 digest of a token.
		 */
		static std::string __digest(const std::string& token);

		/**
		 * Get whether a token is accepted.
		 */
		bool __accepts(const std::string& token);

		/**
		 * Get whether an address is being refused, dropping any count whose window has passed.
		 * @note Must be called with _lock held.
		 */
		bool __isLimited(const std::string& address, long long now);

		/// SHA-256 digest of each accepted token.
		std::vector<std::string> _digests;

		/// Failures by client address. Guarded by _lock.
		std::map<std::string, FailureCount> _failures;

		/// Generic lock.
		ThreadMutex _lock;
};

#endif
