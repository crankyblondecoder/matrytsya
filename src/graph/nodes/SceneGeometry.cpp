#include "SceneGeometry.hpp"

#include "../GraphHiveSceneSurface.hpp"

std::atomic<unsigned> SceneGeometry::VertexGroup::_nextId{0};

SceneGeometry::~SceneGeometry()
{
}

SceneGeometry::VertexGroup& SceneGeometry::__groupForVisibility(VertexVisibility visibility)
{
	// Note: This function needs to be externally synchronised.

	if(_vertexGroups.empty() || _vertexGroups.back().visibility != visibility)
	{
		_vertexGroups.push_back(VertexGroup{.visibility = visibility});
	}

	return _vertexGroups.back();
}

void SceneGeometry::addVertexes(std::vector<Vertex> vertexesToAdd, VertexVisibility visibility)
{
	{ SYNC(_lock)

		VertexGroup& group = __groupForVisibility(visibility);

		group.vertexes.insert(group.vertexes.end(), vertexesToAdd.begin(), vertexesToAdd.end());
	}

	_bumpVersion();
}

void SceneGeometry::addVertexes(double* rawData, unsigned length, VertexVisibility visibility)
{
	// Unpacked outside the lock so that only the append itself is synchronised.
	std::vector<Vertex> vertexesToAdd;

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

		vertexesToAdd.push_back(newVertex);
	}

	{ SYNC(_lock)

		VertexGroup& group = __groupForVisibility(visibility);

		group.vertexes.insert(group.vertexes.end(), vertexesToAdd.begin(), vertexesToAdd.end());
	}

	_bumpVersion();
}

std::size_t SceneGeometry::getVertexCount() const
{
	std::size_t count = 0;

	{ SYNC(_lock)

		for(const VertexGroup& vertGroup : _vertexGroups)
		{
			count += vertGroup.vertexes.size();
		}
	}

	return count;
}

void SceneGeometry::populateSurface(Handle<GraphHiveSceneSurface> surface, unsigned nodeId, bool pokeable)
{
	if(!surface.isValid()) return;

	// Copied under the lock so that the surface, which is external to this, is never populated while the
	// vertex store is held.
	std::vector<VertexGroup> groupsToPopulate;

	{ SYNC(_lock)

		groupsToPopulate = _vertexGroups;
	}

	GraphHiveSceneSurface* surfacePtr = surface.getInstance();

	for(const VertexGroup& vertGroup : groupsToPopulate)
	{
		surfacePtr -> addVertexes(vertGroup.vertexes, vertGroup.id, nodeId, getVersion(), pokeable, vertGroup.visibility);
	}
}
