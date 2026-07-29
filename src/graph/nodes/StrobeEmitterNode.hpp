#ifndef STROBE_EMITTER_NODE_H
#define STROBE_EMITTER_NODE_H

#include "../GraphSerialisedActionNode.hpp"

/**
 * Graph node that emits strobe actions from itself.
 * @note This class is only intended to be inherited and not directly part of the graph.
 */
class StrobeEmitterNode : public GraphSerialisedActionNode
{
    public:

		/**
		 * @param serialiseActions Forwarded to GraphSerialisedActionNode. See its constructor for details.
		 */
        StrobeEmitterNode(bool serialiseActions = false);

		/**
		 * Emit a single strobe action from this node immediately.
		 */
		virtual void emitStrobe();

	protected:

		// Ref counted.
        virtual ~StrobeEmitterNode() = 0;

    private:

        // Do not allow copying.
        StrobeEmitterNode(const StrobeEmitterNode& copyFrom);
        StrobeEmitterNode& operator= (const StrobeEmitterNode& copyFrom);
};

#endif
