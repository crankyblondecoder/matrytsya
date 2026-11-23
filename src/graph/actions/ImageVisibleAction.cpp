#include "ImageVisibleAction.hpp"

#include "../actionTargets/ImageVisibleActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

#include <utility>

ImageVisibleAction::~ImageVisibleAction()
{
}

ImageVisibleAction::ImageVisibleAction(Handle<GraphNode> initNode, bool visible,
	std::vector<unsigned> edgeTraversalKeys)
	: GraphAction(initNode, _startingEnergy, 1, false, std::move(edgeTraversalKeys)), _visible(visible)
{
	_addFlag(IMAGE_VISIBLE_GRAPH_ACTION, true);
}

bool ImageVisibleAction::_apply(GraphNode* target)
{
	ImageVisibleActionTarget* imageVisibleTarget = target -> getImageVisibleActionTarget();

	if(imageVisibleTarget)
	{
		imageVisibleTarget -> setImageVisible(_visible);
	}

	return false;
}

bool ImageVisibleAction::_starting()
{
	return true;
}

void ImageVisibleAction::_complete()
{
}
