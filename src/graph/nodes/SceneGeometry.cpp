#include "SceneGeometry.hpp"

#include "../GraphHiveSceneSurface.hpp"

std::atomic<unsigned> SceneGeometry::VertexGroup::_nextId{0};

SceneGeometry::~SceneGeometry()
{
}

SceneGeometry::VertexGroup& SceneGeometry::__groupForVisibility(VertexVisibility visibility)
{
	if(_vertexGroups.empty() || _vertexGroups.back().visibility != visibility)
	{
		_vertexGroups.push_back(VertexGroup{.visibility = visibility});
	}

	return _vertexGroups.back();
}

void SceneGeometry::addVertexes(std::vector<Vertex> vertexesToAdd, VertexVisibility visibility)
{
	VertexGroup& group = __groupForVisibility(visibility);

	group.vertexes.insert(group.vertexes.end(), vertexesToAdd.begin(), vertexesToAdd.end());

	_bumpVersion();

	_vertexesChanged();
}

void SceneGeometry::addVertexes(double* rawData, unsigned length, VertexVisibility visibility)
{
	VertexGroup& group = __groupForVisibility(visibility);

	for(unsigned index = 0; index + VERTEX_SERIAL_SIZE <= length;)
	{
		// Pack into a Vertex.
		Vertex newVertex {

			// Position: X, Y, Z
			{rawData[index++], rawData[index++], rawData[index++]},

			// Colour: R, G, B, A
			//std::byte colour[4];
			{
				static_cast<std::byte>(rawData[index++]),
				static_cast<std::byte>(rawData[index++]),
				static_cast<std::byte>(rawData[index++]),
				static_cast<std::byte>(rawData[index++])
			},

			// Texture coordinates: U, V
			// double texCoords[2];
			{rawData[index++], rawData[index++]},

			// Normal (must be normalised): X, Y, Z
			//double normal[3];
			{rawData[index++], rawData[index++], rawData[index++]}
		};

		group.vertexes.push_back(newVertex);
	}

	_bumpVersion();

	_vertexesChanged();
}

std::size_t SceneGeometry::getVertexCount() const
{
	std::size_t count = 0;

	for(VertexGroup vertGroup : _vertexGroups)
	{
		count += vertGroup.vertexes.size();
	}

	return count;
}

void SceneGeometry::populateSurface(GraphHandle<GraphHiveSceneSurface> surface, unsigned nodeId, bool pokeable)
{
	if(surface.isValid())
	{
		GraphHiveSceneSurface* surfacePtr = surface.getInstance();

		for(VertexGroup vertGroup : _vertexGroups)
		{
			surfacePtr -> addVertexes(vertGroup.vertexes, vertGroup.id, nodeId, getVersion(), pokeable, vertGroup.visibility);
		}
	}
}
