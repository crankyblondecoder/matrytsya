#ifndef MODEL_CONTEXT_H
#define MODEL_CONTEXT_H

#include <atomic>
#include <string>
#include <vector>

#include "ModelPrompt.hpp"
#include "ModelSystemPrompt.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../util/Handle.hpp"
#include "../util/RefCounted.hpp"

class ModelToolBindings;

// The number of characters a description derived from a context's first prompt is cut to.
#define MODEL_CONTEXT_DESCRIPTION_LENGTH 64

/**
 * Everything a model needs in order to service a request apart from the prompt being made: the system
 * prompts to prepend, the tools the model may call and what has already passed between the model and the
 * caller.
 * @note A context is meant to outlive the individual requests made against it, so that each new prompt is
 *       answered in the light of the ones before it. It is reference counted for that reason: whoever holds
 *       the conversation keeps a handle to it, and each request made in it holds one of its own.
 * @note One request at a time. A conversation is a sequence, so a second request made in a context while
 *       one is still in progress is refused rather than interleaved with it, whichever thread it comes
 *       from. See RequestClaim.
 */
class ModelContext : public RefCounted
{
	public:

		/**
		 * A single tool call a model asked for, paired with what was reported back to it.
		 * @note The arguments and the result are carried as the JSON every provider states them in; what
		 *       differs between providers is only how they are wrapped on the wire.
		 */
		struct ToolCall
		{
			/// Identifier the provider gave the call, used to pair a result with the call it answers.
			/// Empty when the provider correlates by tool name instead, as Ollama does.
			std::string id;

			/// Name of the tool the model called.
			std::string name;

			/// Arguments the model supplied, as a JSON object. Empty when it supplied none.
			std::string arguments;

			/// Result reported back to the model, as JSON. Carries an error when the call could not be
			/// serviced, since the model is given the chance to recover from one.
			std::string result;
		};

		/**
		 * One round of tool calls a model asked for while working towards its response to a prompt.
		 * @note A model may ask for a further round once it has the results of the last, e.g. when the
		 *       argument of one call is the result of another, so the rounds are kept apart rather than
		 *       flattened into one.
		 */
		struct ToolCallRound
		{
			/// Text the model sent alongside the calls. Often empty, as the calls are the point of it.
			std::string content;

			/// Calls the model asked for in this round, in the order it asked for them.
			std::vector<ToolCall> toolCalls;
		};

		/**
		 * A prompt that has already been processed by a model, paired with the tool calls it made while
		 * working on that prompt and the response it finally gave.
		 */
		struct ChatExchange
		{
			/// Prompt that was sent to the model.
			ModelPrompt prompt;

			/// Rounds of tool calls the model asked for before responding, oldest first. Empty when it
			/// answered the prompt outright.
			std::vector<ToolCallRound> toolCallRounds;

			/// Response the model gave to the prompt.
			std::string response;
		};

		/**
		 * Scoped claim of a context for the duration of one request.
		 * @note Held for as long as a request is being serviced, and given up when it leaves scope however
		 *       the request ends, so a request that throws does not leave the context claimed.
		 */
		class RequestClaim
		{
			public:

				/**
				 * Claim a context for one request.
				 * @param context Context to claim. Must be held by a handle for the life of the claim.
				 * @throw AgentException When a request is already in progress in that context.
				 */
				RequestClaim(ModelContext& context);

				~RequestClaim();

			private:

				// Disable copying. A claim is held in one place only, or it means nothing.
				RequestClaim(const RequestClaim& copyFrom);
				RequestClaim& operator= (const RequestClaim& copyFrom);

				/// Context that was claimed.
				ModelContext& _context;
		};

		/**
		 * Create a context within which requests to a model are made.
		 * @param systemPrompts System prompts to prepend to every request made in this context. May be
		 *        empty, leaving the model to answer on its own terms.
		 * @param tools Tool bindings the model may call while servicing a request. May be empty, for a
		 *        model that cannot call tools or is not meant to.
		 */
		ModelContext(std::vector<ModelSystemPrompt> systemPrompts, std::vector<Handle<ModelToolBindings>> tools);

		/**
		 * Get the unique id of this context.
		 * @note Unique across every context ever made in this process, so an id names the one conversation
		 *       it was given for and no other, whether or not that context still exists.
		 */
		unsigned getId();

		/**
		 * Set what the conversation held in this context is about, for the benefit of whoever is picking a
		 * conversation to carry on with.
		 * @param description Description to set. An empty string puts the context back to describing
		 *        itself by its first prompt.
		 */
		void setDescription(std::string description);

		/**
		 * Get what the conversation held in this context is about.
		 * @note Where no description has been set, one is taken from the first prompt made in this
		 *       context, cut to MODEL_CONTEXT_DESCRIPTION_LENGTH characters. The system prompts are no
		 *       help here, as they say what the model is for rather than what this conversation is about,
		 *       so they are left out of it.
		 * @returns The description. Empty only where none was set and no prompt has been made yet.
		 */
		std::string getDescription();

		/**
		 * Get the system prompts to prepend to every request made in this context.
		 */
		std::vector<ModelSystemPrompt> getSystemPrompts();

		/**
		 * Get the tool bindings the model may call while servicing a request.
		 */
		std::vector<Handle<ModelToolBindings>> getTools();

		/**
		 * Get the prompts already processed in this context, along with the tool calls made while working
		 * on each and the responses given to them, oldest first.
		 */
		std::vector<ChatExchange> getChatHistory();

		/**
		 * Get the response of the most recent chat exchange, the last text the model sent that was meant
		 * for the human rather than for a tool round.
		 * @returns The response, or an empty string when no exchange has been added yet.
		 */
		std::string getLastResponse();

		/**
		 * Get the entire chat history as human readable text, oldest first.
		 * @note A tool call round is only reflected in the text when the model sent content alongside the
		 *       calls; rounds where it said nothing to the human are left out entirely, since the calls and
		 *       their results are not themselves human readable.
		 */
		std::string getReadableChatHistory();

		/**
		 * Append a processed prompt, the tool calls made while working on it and the response given to it
		 * to the chat history.
		 * @param prompt Prompt that was processed.
		 * @param toolCallRounds Rounds of tool calls the model asked for before responding, oldest first.
		 * @param response Response the model gave to the prompt.
		 */
		void addChatExchange(ModelPrompt prompt, std::vector<ToolCallRound> toolCallRounds,
			std::string response);

	protected:

		// Required by ref counting.
		virtual ~ModelContext();

	private:

		// Disable copying.
		ModelContext(const ModelContext& copyFrom);
		ModelContext& operator= (const ModelContext& copyFrom);

		/**
		 * Take the context for one request, if no other request holds it.
		 * @returns True when the context was taken, false when a request already holds it.
		 */
		bool __claim();

		/**
		 * Give up the context taken by __claim().
		 */
		void __release();

		/// Counter used to derive each context's unique id.
		static std::atomic<unsigned> _nextId;

		/// Unique id of this context.
		unsigned _id;

		/// What the conversation held in this context is about. Empty where it is to be described by its
		/// first prompt instead.
		std::string _description;

		/// System prompts to prepend to every request made in this context. Fixed at construction, so no
		/// lock is needed to read it.
		std::vector<ModelSystemPrompt> _systemPrompts;

		/// Tool bindings the model may call while servicing a request. Fixed at construction, so no lock is
		/// needed to read it.
		std::vector<Handle<ModelToolBindings>> _tools;

		/// Prompts already processed and the responses given to them, oldest first.
		std::vector<ChatExchange> _chatHistory;

		/// Whether a request is being serviced in this context.
		bool _requestInProgress = false;

		ThreadMutex _lock;
};

#endif
