#include "TextChangeAction.hpp"

#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

#include <iterator>
#include <string.h>
#include <utility>

namespace
{
	const TextChangeOperation OPERATIONS[] = {TextChangeOperation::REPLACE, TextChangeOperation::APPEND,
		TextChangeOperation::PREPEND};

	/// Name of each of OPERATIONS, in the same order.
	const char* const OPERATION_NAMES[] = {"REPLACE", "APPEND", "PREPEND"};
}

TextChangeAction::~TextChangeAction()
{
}

TextChangeAction::TextChangeAction(Handle<GraphNode> initNode, TextChangeOperation operation, std::string text,
	std::string nodeName, std::vector<unsigned> edgeTraversalKeys)
	: GraphAction(initNode, _startingEnergy, 1, false, std::move(edgeTraversalKeys)), _operation{operation},
	  _text{std::move(text)}, _nodeName{std::move(nodeName)}
{
	_addFlag(TEXT_CHANGE_GRAPH_ACTION, true);
}

const char* TextChangeAction::operationName(TextChangeOperation operation)
{
	for(size_t i = 0; i < std::size(OPERATIONS); i++)
	{
		if(OPERATIONS[i] == operation) return OPERATION_NAMES[i];
	}

	return OPERATION_NAMES[0];
}

bool TextChangeAction::operationFromName(const char* name, TextChangeOperation& operation)
{
	for(size_t i = 0; i < std::size(OPERATIONS); i++)
	{
		if(strcmp(OPERATION_NAMES[i], name) != 0) continue;

		operation = OPERATIONS[i];
		return true;
	}

	return false;
}

bool TextChangeAction::_apply(GraphNode* target)
{
	if(!_nodeName.empty() && target -> getName() != _nodeName) return false;

	TextChangeActionTarget* textChangeTarget = target -> getTextChangeActionTarget();

	if(textChangeTarget)
	{
		textChangeTarget -> requestTextChange(_operation, _text);
	}

	return false;
}

bool TextChangeAction::_starting()
{
	return true;
}

void TextChangeAction::_complete()
{
}
