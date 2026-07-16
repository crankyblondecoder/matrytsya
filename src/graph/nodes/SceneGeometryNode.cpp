#include "SceneGeometryNode.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneGeometryNode::~SceneGeometryNode()
{
}

SceneGeometryNode::SceneGeometryNode()
	: GraphNode()
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
}

void SceneGeometryNode::addVertexes(std::vector<Vertex> vertexesToAdd)
{
	_vertexes.insert(_vertexes.end(), vertexesToAdd.begin(), vertexesToAdd.end());
}

void SceneGeometryNode::addVertexes(double* rawData, unsigned length)
{
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

		_vertexes.push_back(newVertex);
	}
}

void SceneGeometryNode::populateSurface(GraphHandle<GraphHiveSceneSurface> surface)
{
	if(surface.isValid()) surface.getInstance() -> addVertexes(_vertexes, getId(), getId(), getPokeEnabled());
}

void SceneGeometryNode::strobe()
{
}

void SceneGeometryNode::setStrobe(bool flag)
{
	_strobe = flag;
}

SceneActionTarget* SceneGeometryNode::getSceneActionTarget()
{
	return this;
}

StrobeActionTarget* SceneGeometryNode::getStrobeActionTarget()
{
	return this;
}

void SceneGeometryNode::_poked(GraphPoke poke)
{
}
