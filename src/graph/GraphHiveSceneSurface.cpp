#include "GraphHiveSceneSurface.hpp"

#include <utility>

GraphHiveSceneSurface::GraphHiveSceneSurface()
{
}

GraphHiveSceneSurface::~GraphHiveSceneSurface()
{
}

void GraphHiveSceneSurface::addVertexes(const std::vector<Vertex>& vertexes, Transform transform, bool transformAccumulates)
{
	Chunk chunk;

	for(int i = 0; i < 16; i++) chunk.transform[i] = transform[i];

	chunk.accumulateTransform = transformAccumulates;
	chunk.vertexes = vertexes;

	_chunks.push_back(std::move(chunk));
}
