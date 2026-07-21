#ifndef SCENE_TRANSFORM_H
#define SCENE_TRANSFORM_H

#include "../graphSceneElements.hpp"
#include "../GraphHandle.hpp"
#include "../GraphVersioned.hpp"

class GraphHiveSceneSurface;

/**
 * Shared transform store and population API for scene transform nodes. Owns the local transform, so that
 * both the C++ and Lua-scripted transform nodes reuse a single implementation.
 */
class SceneTransform : public GraphVersioned
{
	public:

		virtual ~SceneTransform();

		/**
		 * Set the transform applied to this
		 * @param transform The transform to set.
		 */
		void setTransform(const Transform transform);

		/**
		 * @returns The transform currently applied to this.
		 */
		const Transform& getTransform() const;

		/**
		 * Populate the given scene surface with the transform from this.
		 * @param surface Surface to populate.
		 * @param nodeId Node ID to use when populating surface.
		 */
		void populateSurface(GraphHandle<GraphHiveSceneSurface> surface, unsigned nodeId);

	protected:

		/**
		 * Subclass hook called whenever the transform changes.
		 * @note Implementations must emit the CHANGED notification.
		 */
		virtual void _transformChanged() = 0;

	private:

		/**
		 * The local transform applied to the geometry.
		 * This is standard column-major order of matrix elements.
		 * Default is the identity matrix.
		 */
		Transform _transform = {

			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0
		};
};

#endif
