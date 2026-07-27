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

		/// Temperature that leaves the choice of sampling temperature to the provider.
		static constexpr double PROVIDER_DEFAULT_TEMPERATURE = -1.0;

		/// Highest sampling temperature a request may ask for.
		static constexpr double MAX_TEMPERATURE = 2.0;

		/**
		 * Check that a sampling temperature is one a request may be made at.
		 * @param temperature Temperature to check.
		 * @throw AgentException When the temperature is neither PROVIDER_DEFAULT_TEMPERATURE nor from 0
		 *        up to MAX_TEMPERATURE.
		 * @note Public so that whoever takes a temperature from a caller can refuse a bad one where the
		 *       caller can still do something about it, rather than leaving it to surface at the request
		 *       it eventually breaks.
		 */
		static void checkTemperature(double temperature);

		/**
		 * Create a description of a model request.
		 * @param context Context the request is made within, i.e. the system prompts, tools and chat
		 *        history the model is to service the prompt against.
		 * @param prompt Prompt that is yet to be processed by the model.
		 * @param temperature Sampling temperature the model is to answer at, from 0 up to
		 *        MAX_TEMPERATURE, cut to MODEL_TEMPERATURE_DECIMAL_PLACES decimal places on its way to
		 *        the provider. PROVIDER_DEFAULT_TEMPERATURE asks for no temperature at all, leaving the
		 *        provider to answer at whatever it defaults to.
		 * @throw AgentException When no context was supplied, when the prompt has no text, or when the
		 *        temperature is neither PROVIDER_DEFAULT_TEMPERATURE nor within the permitted range.
		 * @note The context is shared with the caller rather than copied, so that the prompt and the
		 *       response given to it can join the chat history the next request in that context sees. The
		 *       handle keeps it alive for at least as long as the request.
		 */
		ModelRequest(Handle<ModelContext> context, ModelPrompt prompt,
			double temperature = PROVIDER_DEFAULT_TEMPERATURE);

		/**
		 * Get the context the request is made within.
		 * @note Always a valid handle, as a request cannot be created without one.
		 */
		Handle<ModelContext> getContext();

		/**
		 * Get the prompt that is yet to be processed by the model.
		 */
		ModelPrompt getPrompt();

		/**
		 * Get the sampling temperature the model is to answer at.
		 * @returns The temperature, or PROVIDER_DEFAULT_TEMPERATURE where none was asked for, which a
		 *          provider is to take as leaving the choice to it.
		 */
		double getTemperature();

	protected:

	private:

		/// Context the request is made within.
		Handle<ModelContext> _context;

		/// Prompt that is yet to be processed by the model.
		ModelPrompt _prompt;

		/// Sampling temperature the model is to answer at, or PROVIDER_DEFAULT_TEMPERATURE where none
		/// was asked for.
		double _temperature;
};

#endif
