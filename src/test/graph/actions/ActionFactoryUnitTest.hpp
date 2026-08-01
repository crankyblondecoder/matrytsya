#ifndef ACTION_FACTORY_UNIT_TEST_H
#define ACTION_FACTORY_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/actions/ActionFactory.hpp"
#include "../../../graph/actions/PingAction.hpp"
#include "../../../graph/actions/SerialisableAction.hpp"
#include "../../../graph/actions/SerialisableActionPayload.hpp"
#include "../../../graph/actions/TriggerAction.hpp"
#include "../../../graph/actionTargets/PingActionTarget.hpp"
#include "../../../graph/actionTargets/SerialisableActionTarget.hpp"
#include "../../../graph/actionTargets/TriggerActionTarget.hpp"
#include "../../../graph/GraphException.hpp"
#include "../../../util/Handle.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphNode.hpp"
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

    protected:

        void _poked(GraphPoke poke) override {}

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
    Handle<GraphHive> hiveHandle(hive);

    // Nodes must not be stack-allocated; the hive takes ownership of the initial ref.
    PingNode* sourceNode = new PingNode();
    CapturingNode* targetNode = new CapturingNode();

    hive -> addNode(sourceNode);
    hive -> addNode(targetNode);

    Handle<GraphNode> targetHandle(targetNode);

    // Connect source → target so the action is applied (and serialised) at the target.
    sourceNode -> createEdge(targetHandle, {});

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
 * A graph node that accepts both trigger and serialisable actions, retaining the last payload it receives
 * and counting how many times it was triggered.
 */
class TriggerCapturingNode : public GraphNode, public TriggerActionTarget, public SerialisableActionTarget
{
    public:

        virtual ~TriggerCapturingNode()
        {
            if(_capturedPayload) _capturedPayload -> decrRef();
        }

        TriggerCapturingNode() : GraphNode()
        {
            _setEnergyCost(1);
            _addActionFlag(TRIGGER_GRAPH_ACTION);
            _addActionFlag(SERIALISABLE_GRAPH_ACTION);
        }

        void trigger() override { _triggerCount++; }

        bool send(SerialisableActionPayload& payload) override
        {
            // Retain the payload so it outlives the action's _apply() call.
            payload.incrRef();

            if(_capturedPayload) _capturedPayload -> decrRef();

            _capturedPayload = &payload;

            return true;
        }

        TriggerActionTarget* getTriggerActionTarget() override { return this; }

        SerialisableActionTarget* getSerialisableActionTarget() override { return this; }

        SerialisableActionPayload* getCapturedPayload() { return _capturedPayload; }

        unsigned getTriggerCount() { return _triggerCount; }

    protected:

        void _poked(GraphPoke poke) override {}

    private:

        /// Most recent payload delivered via send(); null until the first send().
        SerialisableActionPayload* _capturedPayload = nullptr;

        /// Number of times trigger() has been called.
        unsigned _triggerCount = 0;
};

/**
 * Full round-trip: emit a TriggerAction, let it trigger and then serialise itself into a
 * TriggerCapturingNode, then use ActionFactory to reconstruct the action from the captured payload.
 */
TEST(ActionFactoryTest, TriggerRoundTrip)
{
    GraphHive* hive = new GraphHive(2);
    Handle<GraphHive> hiveHandle(hive);

    // Nodes must not be stack-allocated; the hive takes ownership of the initial ref.
    TriggerCapturingNode* sourceNode = new TriggerCapturingNode();
    TriggerCapturingNode* targetNode = new TriggerCapturingNode();

    hive -> addNode(sourceNode);
    hive -> addNode(targetNode);

    Handle<GraphNode> sourceHandle(sourceNode);
    Handle<GraphNode> targetHandle(targetNode);

    // Connect source → target so the action is applied (triggered and serialised) at the target.
    sourceNode -> createEdge(targetHandle, {});

    TriggerAction* original = new TriggerAction(sourceHandle);
    original -> incrRef();

    original -> start();
    original -> waitOnComplete(0);

    EXPECT_EQ(targetNode -> getTriggerCount(), 1u) << "Target should have been triggered exactly once.";

    ASSERT_NE(targetNode -> getCapturedPayload(), nullptr)
        << "No payload captured; SerialisableAction::_apply may not have reached the target.";

    SerialisableActionPayload* payload = targetNode -> getCapturedPayload();

    EXPECT_EQ(payload -> getSerialisableActionType(), SerialisableAction::SerialisableActionType::TRIGGER)
        << "Payload type should be TRIGGER.";

    SerialisableAction* recreated = ActionFactory::create(targetHandle, *payload);
    ASSERT_NE(recreated, nullptr);

    TriggerAction* recreatedTrigger = dynamic_cast<TriggerAction*>(recreated);
    ASSERT_NE(recreatedTrigger, nullptr) << "Factory should have produced a TriggerAction.";

    recreatedTrigger -> decrRef();
    original -> decrRef();

    hive -> shutdown();
}

/**
 * Factory must throw GraphException when the payload carries an unrecognised action type.
 */
TEST(ActionFactoryTest, ThrowsOnUnknownType)
{
    PingNode* node = new PingNode();
    Handle<GraphNode> handle(node);

    // Payload with UNKNOWN type; size zero is sufficient to trigger the throw before any data is read.
    SerialisableActionPayload* payload = new SerialisableActionPayload(
        SerialisableAction::SerialisableActionType::UNKNOWN, 0u);

    EXPECT_THROW(ActionFactory::create(handle, *payload), GraphException);

    payload -> decrRef();

    // Release the initial construction ref; the handle's decrRef on scope exit will delete the node.
    node -> decrRef();
}

#endif
