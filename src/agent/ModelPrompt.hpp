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
		 */
		ModelPrompt(std::string prompt);

		virtual ~ModelPrompt(){}

		/**
		 * Get the text of this prompt.
		 */
		std::string getPrompt();

	protected:

	private:

		/// The text of the prompt.
		std::string _prompt;
};

#endif
