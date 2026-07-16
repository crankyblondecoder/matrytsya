#ifndef ANIMATE_ACTION_TARGET_H
#define ANIMATE_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a node that can be set as animating.
 */
class AnimateActionTarget : virtual public ActionTarget
{
    public:

        virtual ~AnimateActionTarget() {}

		AnimateActionTarget() {}

		/**
		 * Set whether this target is currently animating.
		 * @param flag True if this target should be marked as animating.
		 * @param serial The serial number of the flag. This will be set to the actions id and, gives an indication
		 *        of time order.
		 */
		virtual void setAnimating(bool flag, unsigned serial) = 0;

	protected:

	private:
};

#endif
