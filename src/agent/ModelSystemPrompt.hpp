#ifndef MODEL_SYSTEM_PROMPT_H
#define MODEL_SYSTEM_PROMPT_H

#include "ModelPrompt.hpp"

/**
 * Describes a system prompt to be sent to an AI model.
 * System prompts are pre-pended to standard prompts for a model request.
 */
class ModelSystemPrompt : public ModelPrompt
{
	public:

		/**
		 * Create a description of a system prompt.
		 * @param prompt Text of the prompt.
		 */
		ModelSystemPrompt(std::string prompt);

	protected:

	private:
};

#endif
