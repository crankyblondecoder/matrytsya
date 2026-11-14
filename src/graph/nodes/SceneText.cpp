#include "SceneText.hpp"

#include "../GraphHiveSceneSurface.hpp"

SceneText::~SceneText()
{
}

void SceneText::setText(const std::string& text)
{
	bool changed = false;

	{ SYNC(_lock)

		if(_text != text)
		{
			_text = text;
			changed = true;
		}
	}

	// Only an actual change is versioned, so that a script setting the same text every strobe doesn't make the
	// surface think there is something new to populate.
	if(changed) _bumpVersion();
}

std::string SceneText::getText() const
{
	{ SYNC(_lock)

		return _text;
	}
}

void SceneText::setPosition(const double position[3])
{
	bool changed = false;

	{ SYNC(_lock)

		for(int axis = 0; axis < 3; axis++)
		{
			if(_position[axis] != position[axis])
			{
				_position[axis] = position[axis];
				changed = true;
			}
		}
	}

	if(changed) _bumpVersion();
}

void SceneText::getPosition(double position[3]) const
{
	{ SYNC(_lock)

		for(int axis = 0; axis < 3; axis++) position[axis] = _position[axis];
	}
}

bool SceneText::setHeight(double height)
{
	// Written so that a NaN fails it too.
	if(!(height > 0.0)) return false;

	bool changed = false;

	{ SYNC(_lock)

		if(_height != height)
		{
			_height = height;
			changed = true;
		}
	}

	if(changed) _bumpVersion();

	return true;
}

double SceneText::getHeight() const
{
	{ SYNC(_lock)

		return _height;
	}
}

void SceneText::setColour(const std::byte colour[4])
{
	bool changed = false;

	{ SYNC(_lock)

		for(int channel = 0; channel < 4; channel++)
		{
			if(_colour[channel] != colour[channel])
			{
				_colour[channel] = colour[channel];
				changed = true;
			}
		}
	}

	if(changed) _bumpVersion();
}

void SceneText::getColour(std::byte colour[4]) const
{
	{ SYNC(_lock)

		for(int channel = 0; channel < 4; channel++) colour[channel] = _colour[channel];
	}
}

bool SceneText::setBackground(const TextBackground& background)
{
	if(!background.isValid()) return false;

	bool changed = false;

	{ SYNC(_lock)

		if(_background != background)
		{
			_background = background;
			changed = true;
		}
	}

	if(changed) _bumpVersion();

	return true;
}

TextBackground SceneText::getBackground() const
{
	{ SYNC(_lock)

		return _background;
	}
}

void SceneText::setVisible(bool visible)
{
	bool changed = false;

	{ SYNC(_lock)

		if(_visible != visible)
		{
			_visible = visible;
			changed = true;
		}
	}

	if(changed) _bumpVersion();
}

bool SceneText::getVisible() const
{
	{ SYNC(_lock)

		return _visible;
	}
}

void SceneText::populateSurface(Handle<GraphHiveSceneSurface> surface, unsigned nodeId)
{
	if(!surface.isValid()) return;

	// Copied under the lock so that the surface, which is external to this, is never populated while the text
	// state is held.
	std::string text;
	double position[3];
	double height;
	std::byte colour[4];
	TextBackground background;
	bool visible;

	{ SYNC(_lock)

		visible = _visible;
		text = _text;
		height = _height;
		background = _background;

		for(int axis = 0; axis < 3; axis++) position[axis] = _position[axis];
		for(int channel = 0; channel < 4; channel++) colour[channel] = _colour[channel];
	}

	if(!visible) return;

	surface.getInstance() -> addText(nodeId, text, position, height, colour, background);
}
