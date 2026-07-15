#ifndef STROBE_ACTION_TARGET_H
#define STROBE_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a node that can be strobed.
 */
class StrobeActionTarget : virtual public ActionTarget
{
    public:

        virtual ~StrobeActionTarget() {}

		StrobeActionTarget() {}

		/**
		 * Strobe the node.
		 */
		virtual void strobe() = 0;

		/**
		 * Set whether this target is currently strobing.
		 * @param flag True if this target should be marked as strobing.
		 */
		virtual void setStrobe(bool flag) = 0;

	protected:

    private:
};

#endif
