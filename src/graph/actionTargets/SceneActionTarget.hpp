#ifndef SCENE_ACTION_TARGET_H
#define SCENE_ACTION_TARGET_H

#include "ActionTarget.hpp"

class GraphHiveSceneSurface;

/**
 * Action target to use for a node that can populate a hive scene surface.
 */
class SceneActionTarget : virtual public ActionTarget
{
    public:

        virtual ~SceneActionTarget() {}

		SceneActionTarget() {}

		/**
		 * Populate the given surface with this node's scene contents.
		 * @param surface Surface to populate.
		 */
		virtual void populateSurface(GraphHiveSceneSurface& surface) = 0;

	protected:

    private:
};

#endif
