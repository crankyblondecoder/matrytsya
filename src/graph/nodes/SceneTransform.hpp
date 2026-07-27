#ifndef SCENE_TRANSFORM_H
#define SCENE_TRANSFORM_H

#include "../graphSceneElements.hpp"
#include "../../util/Handle.hpp"
#include "../GraphVersioned.hpp"
#include "../../thread/thread.hpp"

class GraphHiveSceneSurface;

/**
 * Shared transform store and population API for scene transform nodes. Owns the local transform, so that
 * both the C++ and Lua-scripted transform nodes reuse a single implementation.
 * @note Actions are applied to a node simultaneously, so a script setting the transform can run at the same
 *       time as a scene action reading it. Every access to the transform is therefore synchronised, and the
 *       transform is only ever handed out as a copy.
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
		 * Get the transform currently applied to this.
		 * @note This copies rather than returning a reference, because the stored transform can be
		 *       overwritten by another action at any time.
		 * @param transform Set to the transform currently applied to this.
		 */
		void getTransform(Transform transform) const;

		/**
		 * Populate the given scene surface with the transform from this.
		 * @param surface Surface to populate.
		 * @param nodeId Node ID to use when populating surface.
		 */
		void populateSurface(Handle<GraphHiveSceneSurface> surface, unsigned nodeId);

	private:

		/// Guards _transform. Mutable so that the const read-only accessor can still take it.
		mutable ThreadMutex _lock;

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
