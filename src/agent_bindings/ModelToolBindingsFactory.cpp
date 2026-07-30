#include "ModelToolBindingsFactory.hpp"

#include "AnimateScriptNodeToolBindings.hpp"
#include "BasicHiveToolBindings.hpp"
#include "../agent/ModelToolBindings.hpp"
#include "../graph/GraphHive.hpp"
#include "../graph/nodes/AnimateScriptNode.hpp"

std::vector<Handle<ModelToolBindings>> ModelToolBindingsFactory::getHiveToolBindings(
	AgenticHarness::Capability capability, Handle<GraphHive> hive)
{
	return __createBasicHiveToolBindings(hive);
}

std::vector<Handle<ModelToolBindings>> ModelToolBindingsFactory::getChatToolBindings(
	AgenticHarness::Capability capability, Handle<GraphHive> hive)
{
	return __createBasicHiveToolBindings(hive);
}

std::vector<Handle<ModelToolBindings>> ModelToolBindingsFactory::getAnimateScriptNodeToolBindings(
	AgenticHarness::Capability capability, Handle<AnimateScriptNode> node)
{
	std::vector<Handle<ModelToolBindings>> tools;

	if(!node.isValid()) return tools;

	AnimateScriptNodeToolBindings* bindings = new AnimateScriptNodeToolBindings(node);

	Handle<ModelToolBindings> handle(bindings);

	// The handle carries the reference out to the caller; release the implicit construction ref.
	bindings -> decrRef();

	tools.push_back(handle);

	return tools;
}

std::vector<Handle<ModelToolBindings>> ModelToolBindingsFactory::__createBasicHiveToolBindings(
	Handle<GraphHive> hive)
{
	std::vector<Handle<ModelToolBindings>> tools;

	// Nothing can be built against an instance that has already gone, and bindings handed an invalid handle
	// would have to answer for that on every call made against them.
	if(!hive.isValid()) return tools;

	BasicHiveToolBindings* bindings = new BasicHiveToolBindings(hive);

	Handle<ModelToolBindings> handle(bindings);

	// The handle carries the reference out to the caller; release the implicit construction ref.
	bindings -> decrRef();

	tools.push_back(handle);

	return tools;
}
