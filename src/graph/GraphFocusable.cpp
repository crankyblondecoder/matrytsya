#include "GraphFocusable.hpp"

#include "GraphHiveSceneSurface.hpp"

GraphFocusable::~GraphFocusable()
{
}

bool GraphFocusable::getInitialFocus()
{
	return _initialFocus;
}

void GraphFocusable::setInitialFocus(bool initialFocus)
{
	_initialFocus = initialFocus;
}

double GraphFocusable::getFocusViewportFraction()
{
	return _focusViewportFraction;
}

void GraphFocusable::setFocusViewportFraction(double focusViewportFraction)
{
	_focusViewportFraction = focusViewportFraction;
}

void GraphFocusable::requestFocus(double viewportFraction)
{
	{ SYNC(_focusLock)

		_focusRequestCount++;
		_requestedViewportFraction = viewportFraction;
	}
}

unsigned GraphFocusable::getFocusRequestCount()
{
	{ SYNC(_focusLock)

		return _focusRequestCount;
	}
}

void GraphFocusable::_populateFocusRequest(Handle<GraphHiveSceneSurface> surface, unsigned nodeId)
{
	unsigned focusRequestCount;
	double viewportFraction;

	{ SYNC(_focusLock)

		focusRequestCount = _focusRequestCount;
		viewportFraction = _requestedViewportFraction;
	}

	// A node never asked to be focused on is the common case, and has nothing for the surface to compare.
	if(focusRequestCount > 0 && surface.isValid())
	{
		surface.getInstance() -> addFocusRequest(nodeId, focusRequestCount, viewportFraction);
	}
}
