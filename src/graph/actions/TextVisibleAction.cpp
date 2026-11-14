#include "TextVisibleAction.hpp"

#include "../actionTargets/TextVisibleActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

#include <utility>

TextVisibleAction::~TextVisibleAction()
{
}

TextVisibleAction::TextVisibleAction(Handle<GraphNode> initNode, bool visible, std::vector<unsigned> edgeTraversalKeys)
	: GraphAction(initNode, _startingEnergy, 1, false, std::move(edgeTraversalKeys)), _visible(visible)
{
	_addFlag(TEXT_VISIBLE_GRAPH_ACTION, true);
}

bool TextVisibleAction::_apply(GraphNode* target)
{
	TextVisibleActionTarget* textVisibleTarget = target -> getTextVisibleActionTarget();

	if(textVisibleTarget)
	{
		textVisibleTarget -> setTextVisible(_visible);
	}

	return false;
}

bool TextVisibleAction::_starting()
{
	return true;
}

void TextVisibleAction::_complete()
{
}
