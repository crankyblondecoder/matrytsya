#ifndef PING_ACTION_TARGET_H
#define PING_ACTION_TARGET_H

/**
 * Action target to use for pinging a node.
 */
class PingActionTarget
{
    public:

        virtual ~PingActionTarget() {}

		PingActionTarget() {}

		/** Ping the node. Should return true in the implementation. */
		virtual bool ping() = 0;

	protected:

    private:
};

#endif
