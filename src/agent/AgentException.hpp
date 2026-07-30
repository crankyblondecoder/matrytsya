#ifndef AGENT_EXCEPTION_H
#define AGENT_EXCEPTION_H

#include <string>

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
			MODEL_FETCH_FAILED,
			/// A tool binding was asked to operate on a node that could not be found.
			NODE_NOT_FOUND,
			/// A tool binding was asked to process a binding name it does not expose.
			BINDING_NOT_FOUND,
			/// A tool call was processed without a value for one of its required parameters.
			PARAMETER_NOT_FOUND,
			/// A model request was created without a context, or without any prompt text.
			EMPTY_MODEL_REQUEST,
			/// A request was made in a model context that was already servicing one.
			MODEL_CONTEXT_IN_USE,
			/// A model provider returned a response that could not be parsed, or reported an error.
			MODEL_REQUEST_FAILED,
			/// A model kept requesting tool calls past the permitted number of rounds.
			TOOL_CALL_LIMIT_EXCEEDED,
			/// No model was assigned to a role with the requested capability to service a request.
			NO_CANDIDATE_MODEL,
			/// A model request was given a sampling temperature outside the permitted range.
			INVALID_TEMPERATURE
		};

		virtual ~AgentException(){}

		AgentException(Error error) : Exception(Exception::AGENT), _error{error} {}

		Error getError() {return _error;}

		/**
		 * Get a description of the error in plain language.
		 * @returns The description.
		 * @note Written to be readable by a model as well as in a log, since a tool call that fails is
		 *       reported back to the model that made it.
		 */
		std::string getDescription()
		{
			switch(_error)
			{
				case CONNECTION_FAILED:
					return "The model provider could not be reached.";

				case MODEL_FETCH_FAILED:
					return "The model provider returned a model list that could not be understood.";

				case NODE_NOT_FOUND:
					return "No node of that name exists in the hive.";

				case BINDING_NOT_FOUND:
					return "That tool is not available.";

				case PARAMETER_NOT_FOUND:
					return "A required parameter was not supplied.";

				case EMPTY_MODEL_REQUEST:
					return "The request had no context or no prompt.";

				case MODEL_CONTEXT_IN_USE:
					return "That conversation is already busy with another request.";

				case MODEL_REQUEST_FAILED:
					return "The model provider returned a response that could not be understood.";

				case TOOL_CALL_LIMIT_EXCEEDED:
					return "Too many rounds of tool calls were requested.";

				case NO_CANDIDATE_MODEL:
					return "No model is assigned to that role with that capability.";

				case INVALID_TEMPERATURE:
					return "That sampling temperature is outside the permitted range.";

				case UNKNOWN:
					break;
			}

			return "An unknown error occurred.";
		}

	private:

		Error _error;
};

#endif
