#ifndef BASIC_HIVE_TOOL_BINDINGS_H
#define BASIC_HIVE_TOOL_BINDINGS_H

#include <string>
#include <vector>

#include "../agent/ModelToolBindings.hpp"
#include "../util/Handle.hpp"

class GraphHive;
class ModelToolDefinition;
class ModelToolCallParameterValue;

/**
 * Basic tool bindings that expose simple GraphHive lookups to an AI model.
 */
class BasicHiveToolBindings : public ModelToolBindings
{
	public:

		/**
		 * Create the basic hive tool bindings.
		 * @param hive Hive that the bindings operate against.
		 */
		BasicHiveToolBindings(Handle<GraphHive> hive);

		virtual std::vector<ModelToolDefinition> getModelToolDefinitions() override;

		virtual ModelToolCallParameterValue processBinding(std::string name,
			std::vector<ModelToolCallParameterValue> parameterValues) override;

		/**
		 * Get the names of all the nodes in the bound hive.
		 */
		std::vector<std::string> getNodeNames();

		/**
		 * Get the id of a node in the bound hive, given its name.
		 * @param nodeName Name of the node to find.
		 * @returns Id of the node.
		 * @throw AgentException When no node with that name exists in the bound hive.
		 */
		long long getNodeId(std::string nodeName);

	protected:

		virtual ~BasicHiveToolBindings(){}

	private:

		// Disable copying.
		BasicHiveToolBindings(const BasicHiveToolBindings& copyFrom);
		BasicHiveToolBindings& operator= (const BasicHiveToolBindings& copyFrom);

		/// Hive that the bindings operate against.
		Handle<GraphHive> _hive;
};

#endif
