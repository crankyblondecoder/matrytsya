#ifndef IMAGE_VISIBLE_ACTION_TARGET_H
#define IMAGE_VISIBLE_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a node that displays an image which can be shown or hidden.
 */
class ImageVisibleActionTarget : virtual public ActionTarget
{
	public:

		virtual ~ImageVisibleActionTarget() {}

		ImageVisibleActionTarget() {}

		/**
		 * Set whether the image of this target is shown.
		 * @param visible True to show the image, false to hide it.
		 */
		virtual void setImageVisible(bool visible) = 0;

	protected:

	private:
};

#endif
