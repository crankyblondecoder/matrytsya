#include "ModelRequest.hpp"

#include "AgentException.hpp"
#include "ModelContext.hpp"

ModelRequest::ModelRequest(Handle<ModelContext> context, ModelPrompt prompt) :
	_context{context}, _prompt{prompt}
{
	if(!_context.isValid() || _prompt.getPrompt().empty())
	{
		throw AgentException(AgentException::EMPTY_MODEL_REQUEST);
	}
}

Handle<ModelContext> ModelRequest::getContext()
{
	return _context;
}

ModelPrompt ModelRequest::getPrompt()
{
	return _prompt;
}
