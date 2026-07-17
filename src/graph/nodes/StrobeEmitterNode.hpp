#ifndef STROBE_EMITTER_NODE_H
#define STROBE_EMITTER_NODE_H

#include "../GraphNode.hpp"

/**
 * Graph node that emits strobe actions from itself.
 * @note This class is only intended to be inherited and not directly part of the graph.
 */
class StrobeEmitterNode : public GraphNode
{
    public:

        StrobeEmitterNode();

		/**
		 * Emit a single strobe action from this node immediately.
		 */
		void emitStrobe();

	protected:

		// Ref counted.
        virtual ~StrobeEmitterNode() = 0;

    private:

        // Do not allow copying.
        StrobeEmitterNode(const StrobeEmitterNode& copyFrom);
        StrobeEmitterNode& operator= (const StrobeEmitterNode& copyFrom);
};

#endif
