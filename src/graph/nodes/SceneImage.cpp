#include "SceneImage.hpp"

#include "../GraphHiveSceneSurface.hpp"

SceneImage::~SceneImage()
{
}

void SceneImage::setSource(const ImageSource& source)
{
	bool changed = false;

	{ SYNC(_lock)

		if(_source != source)
		{
			_source = source;
			changed = true;
		}
	}

	if(changed) _bumpVersion();
}

ImageSource SceneImage::getSource() const
{
	{ SYNC(_lock)

		return _source;
	}
}

void SceneImage::setPosition(const double position[3])
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

void SceneImage::getPosition(double position[3]) const
{
	{ SYNC(_lock)

		for(int axis = 0; axis < 3; axis++) position[axis] = _position[axis];
	}
}

bool SceneImage::setHeight(double height)
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

double SceneImage::getHeight() const
{
	{ SYNC(_lock)

		return _height;
	}
}

bool SceneImage::setBackground(const TextBackground& background)
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

TextBackground SceneImage::getBackground() const
{
	{ SYNC(_lock)

		return _background;
	}
}

void SceneImage::setVisible(bool visible)
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

bool SceneImage::getVisible() const
{
	{ SYNC(_lock)

		return _visible;
	}
}

void SceneImage::populateSurface(Handle<GraphHiveSceneSurface> surface, unsigned nodeId)
{
	if(!surface.isValid()) return;

	// Copied under the lock so that the surface, which is external to this, is never populated while the image
	// state is held. The PNG bytes are shared rather than copied.
	ImageSource source;
	double position[3];
	double height;
	TextBackground background;
	bool visible;

	{ SYNC(_lock)

		visible = _visible;
		source = _source;
		height = _height;
		background = _background;

		for(int axis = 0; axis < 3; axis++) position[axis] = _position[axis];
	}

	if(!visible) return;

	surface.getInstance() -> addImage(nodeId, getVersion(), source, position, height, background);
}
