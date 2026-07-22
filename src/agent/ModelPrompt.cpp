#include "ModelPrompt.hpp"

ModelPrompt::ModelPrompt(std::string prompt, bool system) :
	_prompt{prompt}, _system{system}
{
}

std::string ModelPrompt::getPrompt()
{
	return _prompt;
}

bool ModelPrompt::getSystem()
{
	return _system;
}
