#include "ModelPrompt.hpp"

ModelPrompt::ModelPrompt(std::string prompt) :
	_prompt{prompt}
{
}

std::string ModelPrompt::getPrompt()
{
	return _prompt;
}
