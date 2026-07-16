#ifndef STROBE_EMITTER_NODE_H
#define STROBE_EMITTER_NODE_H

#include "../GraphNode.hpp"

/**
 * Graph node that emits strobe actions from itself.
 */
class StrobeEmitterNode : public GraphNode
{
    public:

        virtual ~StrobeEmitterNode();

        StrobeEmitterNode();

		/**
		 * Emit a single strobe action from this node immediately.
		 */
		void emitStrobe();

	protected:

    private:

        // Do not allow copying.
        StrobeEmitterNode(const StrobeEmitterNode& copyFrom);
        StrobeEmitterNode& operator= (const StrobeEmitterNode& copyFrom);
};

#endif
