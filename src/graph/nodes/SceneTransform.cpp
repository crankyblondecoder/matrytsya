#include "SceneTransform.hpp"

#include "../GraphHiveSceneSurface.hpp"

SceneTransform::~SceneTransform()
{
}

void SceneTransform::setTransform(const Transform transform)
{
	{ SYNC(_lock)

		for(int i = 0; i < 16; i++) _transform[i] = transform[i];
	}

	_bumpVersion();
}

void SceneTransform::populateSurface(Handle<GraphHiveSceneSurface> surface, unsigned nodeId)
{
	if(!surface.isValid()) return;

	// Copied first so that the surface, which is external to this, is never populated while the transform
	// is held.
	Transform transformToPopulate;

	getTransform(transformToPopulate);

	surface.getInstance() -> addLocalTransform(transformToPopulate, nodeId);
}

void SceneTransform::getTransform(Transform transform) const
{
	{ SYNC(_lock)

		for(int i = 0; i < 16; i++) transform[i] = _transform[i];
	}
}
