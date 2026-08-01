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

void SceneGeometry::setAgentVisible(bool flag)
{
	// Only an actual change is versioned, so repeatedly setting the same value doesn't make the surface
	// think there is something new to populate.
	if(_agentVisible.exchange(flag) != flag) _stateVersion++;
}

bool SceneGeometry::getAgentVisible() const
{
	return _agentVisible;
}

unsigned SceneGeometry::getSceneVersion()
{
	return GraphVersioned::getVersion() + _stateVersion;
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

	// Read once for the whole populate rather than per group, so that a change part way through can't leave
	// the surface holding a mixture of the old and new state for this node.
	bool agentVisible = _agentVisible;

	// The vertexes alone are versioned here. getSceneVersion() is what decides whether a populate happens at
	// all, so passing that instead would make every agent visible change look like a whole new set of
	// vertexes to the surface.
	unsigned vertexVersion = GraphVersioned::getVersion();

	GraphHiveSceneSurface* surfacePtr = surface.getInstance();

	if(agentVisible) surfacePtr -> setNodeAgentVisible(nodeId);

	for(const VertexGroup& vertGroup : groupsToPopulate)
	{
		surfacePtr -> addVertexes(vertGroup.vertexes, vertGroup.id, nodeId, vertexVersion, pokeable, vertGroup.visibility);
	}
}
