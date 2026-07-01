#ifndef TELEPORT_NODE_H
#define TELEPORT_NODE_H

#include "../GraphNode.hpp"
#include "../GraphNodeLocation.hpp"
#include "../actionTargets/SerialisableActionTarget.hpp"

/**
 * Graph node that teleports any serialisable action it receives to another node, possibly hosted by a
 * different hive, via this node's hive.
 */
class TeleportNode : public GraphNode, public SerialisableActionTarget
{
    public:

        virtual ~TeleportNode();

		/**
		 * @param destination Location of the node that receives the teleported actions.
		 */
        TeleportNode(GraphNodeLocation destination);

		/**
		 * Teleport the payload of a serialisable action to this node's destination.
		 */
		bool send(SerialisableActionPayload& payload) override;

		SerialisableActionTarget* getSerialisableActionTarget() override;

	protected:

    private:

		/// Location that received actions are teleported to.
		GraphNodeLocation _destination;
};

#endif
