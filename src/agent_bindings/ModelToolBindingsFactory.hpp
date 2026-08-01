#ifndef MODEL_TOOL_BINDINGS_FACTORY_H
#define MODEL_TOOL_BINDINGS_FACTORY_H

#include <vector>

#include "../agent/AgenticHarness.hpp"
#include "../graph/GraphToolBindingsFactory.hpp"
#include "../util/Handle.hpp"

class AnimateScriptNode;
class GraphHive;
class GraphNode;
class ModelToolBindings;

/**
 * Supplies the tool bindings of every concrete class this module holds, built against one instance of that
 * class for one role and one model capability.
 * @note Each retrieval builds the bindings it names outright, so what a target class exposes is read off the
 *       one method that serves it. Overriding that method in a subclass is how a hive is given something
 *       other than the standard set.
 * @note This holds no state of its own, so one instance can serve any number of hives and be read by any
 *       number of threads at once.
 */
class ModelToolBindingsFactory : public GraphToolBindingsFactory
{
	public:

		ModelToolBindingsFactory() {}

		/**
		 * @returns BasicHiveToolBindings built against the hive. Empty for a hive that could not be
		 *          referenced.
		 * @note The tools do not vary by capability.
		 */
		virtual std::vector<Handle<ModelToolBindings>> getHiveToolBindings(
			AgenticHarness::Capability capability, Handle<GraphHive> hive) override;

		/**
		 * @returns BasicHiveToolBindings built against the hive, as a chat message that turns on what the hive
		 *          holds needs the same lookups hive level planning does. Empty for a hive that could not be
		 *          referenced.
		 * @note The tools do not vary by capability.
		 */
		virtual std::vector<Handle<ModelToolBindings>> getChatToolBindings(
			AgenticHarness::Capability capability, Handle<GraphHive> hive) override;

		/**
		 * @returns AnimateScriptNodeToolBindings built against the node and serial. Empty for a node that
		 *          could not be referenced.
		 * @note The tools do not vary by capability.
		 */
		virtual std::vector<Handle<ModelToolBindings>> getAnimateScriptNodeToolBindings(
			AgenticHarness::Capability capability, unsigned serial, Handle<AnimateScriptNode> node) override;

		/**
		 * @returns TriggerEmitterToolBindings built against the node. Empty for a node that could not be
		 *          referenced.
		 * @note The tools do not vary by capability.
		 */
		virtual std::vector<Handle<ModelToolBindings>> getGraphNodeToolBindings(
			AgenticHarness::Capability capability, Handle<GraphNode> node) override;

	protected:

		// Required by ref counting.
		virtual ~ModelToolBindingsFactory(){}

	private:

		// Disable copying.
		ModelToolBindingsFactory(const ModelToolBindingsFactory& copyFrom);
		ModelToolBindingsFactory& operator= (const ModelToolBindingsFactory& copyFrom);

		/**
		 * Build the bindings that expose a hive's own lookups, which the hive and chat roles are both given.
		 * @param hive Hive the bindings are to operate against.
		 * @returns The bindings built. Empty for a hive that could not be referenced.
		 */
		std::vector<Handle<ModelToolBindings>> __createBasicHiveToolBindings(Handle<GraphHive> hive);
};

#endif
