#include "SceneTransform.hpp"

#include "../GraphHiveSceneSurface.hpp"

SceneTransform::~SceneTransform()
{
}

void SceneTransform::setTransform(const Transform transform)
{
	for(int i = 0; i < 16; i++) _transform[i] = transform[i];

	_bumpVersion();
}

void SceneTransform::populateSurface(GraphHandle<GraphHiveSceneSurface> surface, unsigned nodeId)
{
	if(surface.isValid()) surface.getInstance() -> addLocalTransform(_transform, nodeId);
}

const Transform& SceneTransform::getTransform() const
{
	return _transform;
}
