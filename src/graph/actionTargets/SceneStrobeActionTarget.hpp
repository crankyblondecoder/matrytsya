#ifndef SCENE_STROBE_ACTION_TARGET_H
#define SCENE_STROBE_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a scene node that can be strobed.
 */
class SceneStrobeActionTarget : virtual public ActionTarget
{
    public:

        virtual ~SceneStrobeActionTarget() {}

		SceneStrobeActionTarget() {}

		/**
		 * Strobe the node.
		 */
		virtual void strobe() = 0;

	protected:

    private:
};

#endif
