#include "GraphHiveSceneSurface.hpp"

#include <utility>

#include "../thread/thread.hpp"
#include "GraphHiveSurface.hpp"

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

	// Determine if two transforms are equal.
	bool transformsEqual(Transform& a, Transform& b)
	{
		return
			a[0] == b[0] && a[4] == b[4] && a[8] == b[8] && a[12] == b[12] &&
			a[1] == b[1] && a[5] == b[5] && a[9] == b[9] && a[13] == b[13] &&
			a[2] == b[2] && a[6] == b[6] && a[10] == b[10] && a[14] == b[14] &&
			a[3] == b[3] && a[7] == b[7] && a[11] == b[11] && a[15] == b[15];
	}
}

GraphHiveSceneSurface::GraphHiveSceneSurface(GraphHandle<SceneRootNode> sceneRootNode)
	: GraphHiveSurface(Type::SCENE_SURFACE), _boundRootNode(sceneRootNode)
{
}

GraphHiveSceneSurface::~GraphHiveSceneSurface()
{
}

void GraphHiveSceneSurface::strobe()
{
	if(_boundRootNode.isValid()) _boundRootNode.getInstance() -> populateSceneSurface(GraphHandle<GraphHiveSceneSurface>(this));
}

void GraphHiveSceneSurface::poke(unsigned nodeId, GraphPoke poke)
{
	// A chunk is uniquely identified by its owning node id together with its chunk id, so both are needed
	// to confirm the poked chunk exists in the current scene before the poke is forwarded to the node.
	bool found = false;

	{ SYNC(_lock)

		for(const Chunk& chunk : _currentScene.chunks)
		{
			if(chunk.nodeId == nodeId && chunk.id == poke.getChunkId())
			{
				found = true;
				break;
			}
		}
	}

	if(found) GraphHiveSurface::poke(nodeId, poke);
}

void GraphHiveSceneSurface::activate()
{
	if(_boundRootNode.isValid()) _boundRootNode.getInstance() -> populateSceneSurface(GraphHandle<GraphHiveSceneSurface>(this));
}

void GraphHiveSceneSurface::_populateStart()
{
	_chunks.clear();
	_chunkUpdates.clear();
	_unchangedChunkIndexes.clear();
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
	// Determine if the scene has changed.
	bool sceneChanged = false;

	{ SYNC(_lock)

		unsigned numTransf = _modelTransforms.size();

		// Note: It is assumed that _unchangedChunkIndexes would never contain a duplicate and if the sizes of this
		//       vector and the current scenes chunk vector are the same then they match exactly.

		sceneChanged = numTransf != _currentScene.modelTransforms.size() || _chunkUpdates.size() > 0 ||
			_chunks.size() > 0 || _unchangedChunkIndexes.size() != _currentScene.chunks.size();

		if(!sceneChanged && numTransf)
		{
			// Do exact check on transforms.
			for(unsigned index = 0; index < numTransf; index++)
			{
				if(!transformsEqual(_modelTransforms[index].transform, _currentScene.modelTransforms[index].transform))
				{
					sceneChanged = true;
					break;
				}
			}
		}
	}

	if(sceneChanged)
	{
		{ SYNC(_lock)

			// Add updated and unchanged chunks into the current chunks array.

			for(unsigned unchangedIndex : _unchangedChunkIndexes)
			{
				_chunks.push_back(std::move(_currentScene.chunks[unchangedIndex]));
			}

			for(ChunkUpdate update : _chunkUpdates)
			{
				Chunk& chunk = _currentScene.chunks[update.sceneChunkIndex];

				chunk.version = update.version;
				chunk.modelTransformIndex = update.modelTransformIndex;
				chunk.pokeable = update.pokeable;
				chunk.visibility = update.visibility;

				_chunks.push_back(std::move(chunk));
			}

			// Move the built chunks and transforms into the scene to make the new scene.
			// Assume this clears the build vectors.
			_currentScene.chunks = std::move(_chunks);
			_currentScene.modelTransforms = std::move(_modelTransforms);
		}

		// Notify that the surface has changed.
		_emitSurfaceChanged();
	}
}

void GraphHiveSceneSurface::addVertexes(const std::vector<Vertex>& vertexes, unsigned chunkId, unsigned nodeId,
	unsigned version, bool pokeable, SceneGeometry::VertexVisibility visibility)
{
	bool found = false;
	unsigned foundIndex = 0;
	unsigned modelTransformIndex = 0;

	{ SYNC(_lock)

		modelTransformIndex = _modelTransforms.size() - 1;

		unsigned numChunks = _currentScene.chunks.size();

		// Look for the chunk in the current scene that still has the same vertexes version.
		for(;foundIndex < numChunks; foundIndex++)
		{
			Chunk& chunk = _currentScene.chunks[foundIndex];

			if(chunk.id == chunkId && chunk.nodeId == nodeId && chunk.vertexVersion == version)
			{
				// Look for whether it is an update or unchanged.
				if(chunk.pokeable == pokeable && chunk.modelTransformIndex == modelTransformIndex &&
					chunk.visibility == visibility)
				{
					// Unchanged.
					_unchangedChunkIndexes.push_back(foundIndex);
				}
				else
				{
					// Requires update and version bump.
					_chunkUpdates.push_back({

						.sceneChunkIndex = foundIndex,
						.version = chunk.version + 1,
						.modelTransformIndex = modelTransformIndex,
						.pokeable = pokeable,
						.visibility = visibility
					});
				}

				found = true;
				break;
			}
		}
	}

	if(!found)
	{
		Chunk chunk;

		chunk.id = chunkId;
		chunk.nodeId = nodeId;
		chunk.vertexVersion = version;
		chunk.pokeable = pokeable;
		chunk.vertexes = vertexes;
		chunk.visibility = visibility;
		chunk.modelTransformIndex = modelTransformIndex;

		{ SYNC(_lock)

			_chunks.push_back(std::move(chunk));
		}
	}
}

void GraphHiveSceneSurface::addLocalTransform(const Transform& transform, unsigned id)
{
	{ SYNC(_lock)

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
}

void GraphHiveSceneSurface::setInitialFocusNode(unsigned nodeId, double focusViewportFraction)
{
	{ SYNC(_lock)

		_currentScene.hasInitialFocusNode = true;
		_currentScene.initialFocusNodeId = nodeId;
		_currentScene.focusViewportFraction = focusViewportFraction;
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
