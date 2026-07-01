#ifndef ACTION_FACTORY_UNIT_TEST_H
#define ACTION_FACTORY_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/actions/ActionFactory.hpp"
#include "../../../graph/actions/PingAction.hpp"
#include "../../../graph/actions/SerialisableAction.hpp"
#include "../../../graph/actions/SerialisableActionPayload.hpp"
#include "../../../graph/actionTargets/PingActionTarget.hpp"
#include "../../../graph/actionTargets/SerialisableActionTarget.hpp"
#include "../../../graph/GraphException.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphHiveHandle.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/GraphNodeHandle.hpp"
#include "../../../graph/graphActionFlagRegister.hpp"
#include "../../../graph/nodes/PingNode.hpp"

/**
 * A graph node that accepts both ping and serialisable actions, retaining the last payload it receives.
 * Used to intercept the serialised form of a SerialisableAction as it traverses the graph.
 */
class CapturingNode : public GraphNode, public PingActionTarget, public SerialisableActionTarget
{
    public:

        virtual ~CapturingNode()
        {
            if(_capturedPayload) _capturedPayload -> decrRef();
        }

        CapturingNode() : GraphNode()
        {
            _setEnergyCost(1);
            _addActionFlag(PING_GRAPH_ACTION);
            _addActionFlag(SERIALISABLE_GRAPH_ACTION);
        }

        bool ping() override { return true; }

        bool send(SerialisableActionPayload& payload) override
        {
            // Retain the payload so it outlives the action's _apply() call.
            payload.incrRef();

            if(_capturedPayload) _capturedPayload -> decrRef();

            _capturedPayload = &payload;

			return true;
        }

        PingActionTarget* getPingActionTarget() override { return this; }

        SerialisableActionTarget* getSerialisableActionTarget() override { return this; }

        SerialisableActionPayload* getCapturedPayload() { return _capturedPayload; }

    private:

        /// Most recent payload delivered via send(); null until the first send().
        SerialisableActionPayload* _capturedPayload = nullptr;
};

/**
 * Full round-trip: emit a PingAction, let it serialise itself into a CapturingNode, then use
 * ActionFactory to reconstruct the action from the captured payload and verify the ping count is
 * preserved.
 */
TEST(ActionFactoryTest, PingRoundTrip)
{
    GraphHive* hive = new GraphHive(2);
    GraphHiveHandle hiveHandle(hive);

    // Nodes must not be stack-allocated; the hive takes ownership of the initial ref.
    PingNode* sourceNode = new PingNode();
    CapturingNode* targetNode = new CapturingNode();

    hive -> addNode(sourceNode);
    hive -> addNode(targetNode);

    GraphNodeHandle targetHandle(targetNode);

    // Connect source → target so the action is applied (and serialised) at the target.
    sourceNode -> createEdge(targetHandle);

    PingAction* original = sourceNode -> emitPing(true);

    ASSERT_NE(targetNode -> getCapturedPayload(), nullptr)
        << "No payload captured; SerialisableAction::_apply may not have reached the target.";

    SerialisableActionPayload* payload = targetNode -> getCapturedPayload();

    EXPECT_EQ(payload -> getSerialisableActionType(), SerialisableAction::SerialisableActionType::PING)
        << "Payload type should be PING.";

    SerialisableAction* recreated = ActionFactory::create(targetHandle, *payload);
    ASSERT_NE(recreated, nullptr);

    PingAction* recreatedPing = dynamic_cast<PingAction*>(recreated);
    ASSERT_NE(recreatedPing, nullptr) << "Factory should have produced a PingAction.";

    EXPECT_EQ(recreatedPing -> getPingCount(), original -> getPingCount())
        << "Deserialised ping count does not match the serialised original.";

    recreatedPing -> decrRef();
    original -> decrRef();

    hive -> shutdown();
}

/**
 * Factory must throw GraphException when the payload carries an unrecognised action type.
 */
TEST(ActionFactoryTest, ThrowsOnUnknownType)
{
    PingNode* node = new PingNode();
    GraphNodeHandle handle(node);

    // Payload with UNKNOWN type; size zero is sufficient to trigger the throw before any data is read.
    SerialisableActionPayload* payload = new SerialisableActionPayload(
        SerialisableAction::SerialisableActionType::UNKNOWN, 0u);

    EXPECT_THROW(ActionFactory::create(handle, *payload), GraphException);

    payload -> decrRef();

    // Release the initial construction ref; the handle's decrRef on scope exit will delete the node.
    node -> decrRef();
}

#endif
