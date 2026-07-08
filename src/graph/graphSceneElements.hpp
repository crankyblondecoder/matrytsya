#ifndef GRAPH_SCENE_ELEMENTS_H
#define GRAPH_SCENE_ELEMENTS_H

#include <cstddef>

// A collection of data structures that are used to assemble a 3D graph scene.

// Number of serial values in a Vertex.
#define VERTEX_SERIAL_SIZE 12

/**
 * Data structure that describes a single vertex.
 */
struct Vertex
{
	/// Position: X, Y, Z
	double posn[3];

	/// Colour: R, G, B, A
	std::byte colour[4];

	/// Texture coordinates: U, V
	double texCoords[2];

	/// Normal (must be normalised): X, Y, Z
	double normal[3];
};

using Transform = double[16];

#endif

