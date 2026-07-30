#ifndef GRAPH_TOOL_BINDINGS_FACTORY_H
#define GRAPH_TOOL_BINDINGS_FACTORY_H

#include <vector>

#include "../agent/AgenticHarness.hpp"
#include "../util/Handle.hpp"
#include "../util/RefCounted.hpp"

class AnimateScriptNode;
class GraphHive;
class ModelToolBindings;

/**
 * Supplies the tool bindings that a concrete class within a hive makes available to an AI model, built
 * against one instance of that class and for one model capability.
 * @note This is the graph facing half of the factory. The concrete implementation lives in the
 *       agent_bindings module, which depends on this module, so a hive is given its factory from outside
 *       rather than building one itself.
 * @note There is a retrieval method per target class rather than one method taking a base class, so the
 *       concrete class a set of bindings is built against is known statically and no cast is needed to
 *       recover it.
 */
class GraphToolBindingsFactory : public RefCounted
{
	public:

		/**
		 * Get the tool bindings that a hive makes available.
		 * @param capability Capability of the model the bindings are being requested for.
		 * @param hive Hive the bindings are to operate against.
		 * @returns The bindings the hive makes available for the capability. Empty where there are none.
		 * @note Fixed to AgenticHarness::Role::HIVE, as a hive's own bindings are only ever reached through
		 *       the hive role.
		 */
		virtual std::vector<Handle<ModelToolBindings>> getHiveToolBindings(
			AgenticHarness::Capability capability, Handle<GraphHive> hive) = 0;

		/**
		 * Get the tool bindings that a hive makes available to the chat assistant.
		 * @param capability Capability of the model the bindings are being requested for.
		 * @param hive Hive the bindings are to operate against.
		 * @returns The bindings the chat assistant is given for the capability. Empty where there are none.
		 * @note Fixed to AgenticHarness::Role::CHAT. Kept apart from getHiveToolBindings() because what a
		 *       chat message may reach and what hive level planning may reach are answers to different
		 *       questions, even where they currently come to the same set.
		 */
		virtual std::vector<Handle<ModelToolBindings>> getChatToolBindings(
			AgenticHarness::Capability capability, Handle<GraphHive> hive) = 0;

		/**
		 * Get the tool bindings that an animate script node makes available.
		 * @param capability Capability of the model the bindings are being requested for.
		 * @param node Node the bindings are to operate against.
		 * @returns The bindings the node makes available for the capability. Empty where there are none.
		 * @note Fixed to AgenticHarness::Role::NODE, as a node's own bindings are only ever reached through
		 *       the node role.
		 */
		virtual std::vector<Handle<ModelToolBindings>> getAnimateScriptNodeToolBindings(
			AgenticHarness::Capability capability, Handle<AnimateScriptNode> node) = 0;

	protected:

		GraphToolBindingsFactory() {}

		// Required by ref counting.
		virtual ~GraphToolBindingsFactory() {}

	private:

		// Disable copying.
		GraphToolBindingsFactory(const GraphToolBindingsFactory& copyFrom);
		GraphToolBindingsFactory& operator= (const GraphToolBindingsFactory& copyFrom);
};

#endif
