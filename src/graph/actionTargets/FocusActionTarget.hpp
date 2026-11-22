#ifndef FOCUS_ACTION_TARGET_H
#define FOCUS_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a node whose visual object the views of a scene can be asked to bring into focus.
 */
class FocusActionTarget : virtual public ActionTarget
{
	public:

		virtual ~FocusActionTarget() {}

		FocusActionTarget() {}

		/**
		 * Ask the views of every scene this target is collected into to bring its visual object into focus.
		 * @param viewportFraction If 0, the view focuses the same way a viewer right-clicking the object would.
		 *        Otherwise, the fraction (greater than 0, no greater than 1) of the view's height the object's
		 *        bounds are fitted to, the same way a view first focuses on an initial-focus node.
		 * @note Only a request: it reaches a scene surface the next time that surface is populated.
		 */
		virtual void requestFocus(double viewportFraction = 0) = 0;

	protected:

	private:
};

#endif
