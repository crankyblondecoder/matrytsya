#ifndef SCENE_GEOMETRY_H
#define SCENE_GEOMETRY_H

#include <atomic>
#include <cstddef>
#include <vector>

#include "../graphSceneElements.hpp"
#include "../GraphHandle.hpp"
#include "../GraphVersioned.hpp"

class GraphHiveSceneSurface;

/**
 * Shared vertex store and population API for scene geometry nodes. Owns the vertex list and the two
 * ways of appending to it (Vertex structs or raw serialised data), so that both the C++ and Lua-scripted
 * geometry nodes reuse a single implementation.
 */
class SceneGeometry : public GraphVersioned
{
	public:

		virtual ~SceneGeometry();

		enum class VertexVisibility
		{
			/// Always visible.
			ALWAYS,

			/// Visible when in a grabbed state. This typically maps to actions like mouse button held down.
			GRABBED,

			/// Visible when being dragged. This typically maps to a mouse down then move type of action.
			DRAGGING,

			/// Visible when being hovered over. This typically maps to a non-button down mouse over movement.
			HOVERED_OVER
		};

		struct VertexGroup
		{
			/// Id unique over all vertex groups.
			unsigned id = _nextId++;

			/**
			 * The vertexes that make up a group of vertexes. Multiple groups combined make up a scene objects
			 * geometry.
			 * Each linearly ordered triplet of vertexes defines a triangle with standard counter-clockwise winding
			 * order for the front face.
			 * @note There is no indexing at this stage.
			 */
			std::vector<Vertex> vertexes;

			VertexVisibility visibility;

			private:

				/// Counter used to derive each vertex group's unique id.
				static std::atomic<unsigned> _nextId;
		};

		/**
		 * Add vertexes to the list of vertexes for this scene node.
		 * @note If the visibility doesn't match the current vertex group, a new group will be created.
		 * @param vertexesToAdd List of vertexes to add.
		 * @param visibility What kind of visibility these vertexes have.
		 */
		void addVertexes(std::vector<Vertex> vertexesToAdd, VertexVisibility visibility = VertexVisibility::ALWAYS);

		/**
		 * Add vertexes as an array of raw data.
		 * @note If the visibility doesn't match the current vertex group, a new group will be created.
		 * @param rawData Array of raw data that matches the Vertex struct. Multiple vertexes can be defined.
		 * @param length Length of raw data array. Must be in multiples of VERTEX_SERIAL_SIZE. An incomplete vertex
		 *        at the end of the array will simply be discarded rather than throw an exception.
		 */
		void addVertexes(double* rawData, unsigned length, VertexVisibility visibility = VertexVisibility::ALWAYS);

		/**
		 * @returns The number of vertexes currently held.
		 */
		std::size_t getVertexCount() const;

		/**
		 * Populate the given scene surface with vertexes from this.
		 * @param surface Surface to populate.
		 * @param nodeId Node ID to use when populating surface.
		 * @param pokeable Pokeable flag to set when populating surface.
		 */
		void populateSurface(GraphHandle<GraphHiveSceneSurface> surface, unsigned nodeId, bool pokeable);

	protected:

		/**
		 * Subclass hook called whenever vertexes are added.
		 * @note Implementations must emit the CHANGED notification.
		 */
		virtual void _vertexesChanged() = 0;

	private:

		/**
		 * Get the vertex group to append new vertexes to for the given visibility, creating a new group
		 * on the end of _vertexGroups if the last group present doesn't already have that visibility.
		 * @param visibility Visibility of the vertexes about to be appended.
		 * @returns The vertex group to append to.
		 */
		VertexGroup& __groupForVisibility(VertexVisibility visibility);

		/**
		 * The groups of vertexes that make up the scene object this node defines.
		 */
		std::vector<VertexGroup> _vertexGroups;
};

#endif
