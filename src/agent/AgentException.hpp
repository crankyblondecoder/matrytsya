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
			/// The Lua function implementing a script defined tool was missing, raised, or returned a value
			/// that did not match the return type the script declared for it.
			SCRIPT_TOOL_FAILED,
			/// A tool call was processed without a value for one of its required parameters.
			PARAMETER_NOT_FOUND,
			/// A model request was created without a context, or without any prompt text.
			EMPTY_MODEL_REQUEST,
			/// A request was made in a model context that was already servicing one.
			MODEL_CONTEXT_IN_USE,
			/// A model provider returned a response to a request that could not be parsed.
			MODEL_REQUEST_FAILED,
			/// A model kept requesting tool calls past the permitted number of rounds.
			TOOL_CALL_LIMIT_EXCEEDED,
			/// No model was assigned to a role with the requested capability to service a request.
			NO_CANDIDATE_MODEL,
			/// A model request was given a sampling temperature outside the permitted range.
			INVALID_TEMPERATURE,
			/// A request was abandoned because the thread servicing it was asked to stop.
			REQUEST_ABORTED,
			/// A model provider rejected the credentials it was given.
			AUTHENTICATION_FAILED,
			/// A model provider, or the host serving a model behind it, turned a request away for being made
			/// too often.
			MODEL_RATE_LIMITED,
			/// A model provider refused a request because the account has too little credit to pay for it.
			INSUFFICIENT_CREDITS,
			/// A model provider answered with an error of its own, of a kind no more specific error covers.
			MODEL_PROVIDER_ERROR
		};

		virtual ~AgentException(){}

		/**
		 * @param error What went wrong.
		 * @param detail What is known beyond the error itself, e.g. the reason a provider gave for turning a
		 *        request away. Empty when the error alone says all that is known.
		 */
		AgentException(Error error, std::string detail = "") : Exception(Exception::AGENT), _error{error},
			_detail{detail} {}

		Error getError() {return _error;}

		/**
		 * Get what is known about the error beyond the error itself.
		 * @returns The detail given when the exception was raised, or empty if none was.
		 */
		const std::string& getDetail() {return _detail;}

		/**
		 * Get a description of the error in plain language, followed by its detail when there is one.
		 * @returns The description.
		 * @note Written to be readable by a model as well as in a log, since a tool call that fails is
		 *       reported back to the model that made it.
		 */
		std::string getDescription()
		{
			std::string description = __getErrorDescription();

			if(!_detail.empty()) description += " Details: " + _detail;

			return description;
		}

	private:

		/**
		 * Get a description of the error alone, in plain language.
		 * @returns The description.
		 */
		std::string __getErrorDescription()
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

				case SCRIPT_TOOL_FAILED:
					return "The script implementing that tool failed.";

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

				case REQUEST_ABORTED:
					return "The request was abandoned because the application is shutting down.";

				case AUTHENTICATION_FAILED:
					return "The model provider rejected the API key.";

				case MODEL_RATE_LIMITED:
					return "The model is rate-limited; try again shortly.";

				case INSUFFICIENT_CREDITS:
					return "The model provider account does not have enough credit for that request.";

				case MODEL_PROVIDER_ERROR:
					return "The model provider reported an error.";

				case UNKNOWN:
					break;
			}

			return "An unknown error occurred.";
		}

		Error _error;

		/// What is known about the error beyond the error itself. Empty when nothing is.
		std::string _detail;
};

#endif
