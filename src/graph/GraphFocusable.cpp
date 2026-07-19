#include "GraphFocusable.hpp"

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
