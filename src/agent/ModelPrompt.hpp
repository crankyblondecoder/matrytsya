#ifndef MODEL_PROMPT_H
#define MODEL_PROMPT_H

#include <string>

/**
 * Describes a single prompt to be sent to an AI model.
 */
class ModelPrompt
{
	public:

		/**
		 * Create a description of a prompt.
		 * @param prompt Text of the prompt.
		 * @param system Whether this is a system prompt. System prompts are pre-pended to standard prompts for a
		 *        model request.
		 */
		ModelPrompt(std::string prompt, bool system);

		/**
		 * Get the text of this prompt.
		 */
		std::string getPrompt();

		/**
		 * Whether this is a system prompt.
		 * System prompts are pre-pended to standard prompts for a model request.
		 */
		bool getSystem();

	protected:

	private:

		/// The text of the prompt.
		std::string _prompt;

		/// Whether this is a system prompt.
		bool _system;
};

#endif
