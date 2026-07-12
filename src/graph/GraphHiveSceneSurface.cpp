#include "GraphHiveSceneSurface.hpp"

#include <utility>

#include "../thread/thread.hpp"

namespace
{
	// Multiply two column-major 4x4 transforms: result = a * b.
	void multiplyTransforms(Transform result, const Transform a, const Transform b)
	{
		for(int col = 0; col < 4; col++)
		{
			for(int row = 0; row < 4; row++)
			{
				double sum = 0.0;

				for(int k = 0; k < 4; k++) sum += a[k * 4 + row] * b[col * 4 + k];

				result[col * 4 + row] = sum;
			}
		}
	}
}

GraphHiveSceneSurface::GraphHiveSceneSurface(GraphHandle<SceneRootNode> sceneRootNode) : _boundRootNode(sceneRootNode)
{
}

GraphHiveSceneSurface::~GraphHiveSceneSurface()
{
}

void GraphHiveSceneSurface::activate()
{
	if(_boundRootNode.isValid())
	{
		_boundRootNode.getInstance() -> populateSceneSurface(GraphHandle<GraphHiveSceneSurface>(this));
	}
}

void GraphHiveSceneSurface::_populateStart()
{
	_chunks.clear();
	_modelTransforms.clear();

	ModelTransform identity;

	identity.id = 0;
	identity.transform[0] = 1.0; identity.transform[1] = 0.0; identity.transform[2] = 0.0; identity.transform[3] = 0.0;
	identity.transform[4] = 0.0; identity.transform[5] = 1.0; identity.transform[6] = 0.0; identity.transform[7] = 0.0;
	identity.transform[8] = 0.0; identity.transform[9] = 0.0; identity.transform[10] = 1.0; identity.transform[11] = 0.0;
	identity.transform[12] = 0.0; identity.transform[13] = 0.0; identity.transform[14] = 0.0; identity.transform[15] = 1.0;

	_modelTransforms.push_back(identity);
}

void GraphHiveSceneSurface::_populateEnd()
{
	{ SYNC(_lock)

		// Move the built chunks and transforms into the scene to make the new scene.
		// Assume this clears the build vectors.
		_currentScene.chunks = std::move(_chunks);
		_currentScene.modelTransforms = std::move(_modelTransforms);
	}

	// Notify that the surface has changed.
	_emitSurfaceChanged();
}

void GraphHiveSceneSurface::addVertexes(const std::vector<Vertex>& vertexes, unsigned id)
{
	Chunk chunk;

	chunk.id = id;
	chunk.vertexes = vertexes;

	chunk.modelTransformIndex = _modelTransforms.size() - 1;

	_chunks.push_back(std::move(chunk));
}

void GraphHiveSceneSurface::addLocalTransform(const Transform& transform, unsigned id)
{
	// Look for existing transform that matches the ID first.
	int index = _modelTransforms.size() - 1;

	for(; index >= 0; index--)
	{
		if(_modelTransforms[index].id == id)
		{
			break;
		}
	}

	ModelTransform modelTransform;

	if(index >= 0)
	{
		modelTransform = _modelTransforms[index];
		_modelTransforms.push_back(modelTransform);
	}
	else
	{
		modelTransform.id = id;

		// Pre-multiplies (This is standard OpenGL behaviour).
		multiplyTransforms(modelTransform.transform, transform, _modelTransforms.back().transform);

		_modelTransforms.push_back(modelTransform);
	}
}

GraphHiveSceneSurface::Scene GraphHiveSceneSurface::getScene()
{
	{ SYNC(_lock)

		return _currentScene;
	}
}

void GraphHiveSceneSurface::_close()
{
}
