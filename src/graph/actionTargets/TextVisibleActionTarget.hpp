#ifndef TEXT_VISIBLE_ACTION_TARGET_H
#define TEXT_VISIBLE_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a node that displays text which can be shown or hidden.
 */
class TextVisibleActionTarget : virtual public ActionTarget
{
    public:

        virtual ~TextVisibleActionTarget() {}

		TextVisibleActionTarget() {}

		/**
		 * Set whether the text of this target is shown.
		 * @param visible True to show the text, false to hide it.
		 */
		virtual void setTextVisible(bool visible) = 0;

	protected:

	private:
};

#endif
