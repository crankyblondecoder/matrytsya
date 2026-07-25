#ifndef MODEL_REQUEST_H
#define MODEL_REQUEST_H

#include "ModelPrompt.hpp"
#include "../util/Handle.hpp"

class ModelContext;

/**
 * Describes a full request to be sent to an AI model, comprising the context the request is made within and
 * the single prompt that is yet to be processed by the model.
 */
class ModelRequest
{
	public:

		/**
		 * Create a description of a model request.
		 * @param context Context the request is made within, i.e. the system prompts, tools and chat
		 *        history the model is to service the prompt against.
		 * @param prompt Prompt that is yet to be processed by the model.
		 * @throw AgentException When no context was supplied, or when the prompt has no text.
		 * @note The context is shared with the caller rather than copied, so that the prompt and the
		 *       response given to it can join the chat history the next request in that context sees. The
		 *       handle keeps it alive for at least as long as the request.
		 */
		ModelRequest(Handle<ModelContext> context, ModelPrompt prompt);

		/**
		 * Get the context the request is made within.
		 * @note Always a valid handle, as a request cannot be created without one.
		 */
		Handle<ModelContext> getContext();

		/**
		 * Get the prompt that is yet to be processed by the model.
		 */
		ModelPrompt getPrompt();

	protected:

	private:

		/// Context the request is made within.
		Handle<ModelContext> _context;

		/// Prompt that is yet to be processed by the model.
		ModelPrompt _prompt;
};

#endif
