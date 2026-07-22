#ifndef AGENT_EXCEPTION_H
#define AGENT_EXCEPTION_H

#include "../util/Exception.hpp"

/**
 * Exception raised by the agent module.
 */
class AgentException : public Exception
{
	public:

		enum Error
		{
			UNKNOWN,
			/// Could not connect to a model provider.
			CONNECTION_FAILED,
			/// A model provider returned a model list that could not be parsed.
			MODEL_FETCH_FAILED
		};

		virtual ~AgentException(){}

		AgentException(Error error) : Exception(Exception::AGENT), _error{error} {}

		Error getError() {return _error;}

	private:

		Error _error;
};

#endif
