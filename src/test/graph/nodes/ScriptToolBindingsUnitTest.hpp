#ifndef SCRIPT_TOOL_BINDINGS_UNIT_TEST_H
#define SCRIPT_TOOL_BINDINGS_UNIT_TEST_H

#include <gtest/gtest.h>

#include <string>
#include <variant>
#include <vector>

#include "../../../agent/AgentException.hpp"
#include "../../../agent/AgenticHarness.hpp"
#include "../../../agent/ModelToolBindings.hpp"
#include "../../../agent/ModelToolCallParameterValue.hpp"
#include "../../../agent/ModelToolDefinition.hpp"
#include "../../../agent/ModelToolDefinitionParameter.hpp"
#include "../../../agent_bindings/ModelToolBindingsFactory.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphToolBindingsFactory.hpp"
#include "../../../graph/nodes/AnimateScriptNode.hpp"
#include "../../../graph/nodes/ScriptNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"
#include "../../../graph/nodes/TriggerNode.hpp"
#include "../../../lua/lua.hpp"
#include "../../../util/Handle.hpp"

/**
 * ScriptNode subclass that exposes _getScriptToolBindings() and keeps hold of the core state, so tests can
 * ask for a node's script defined tools and afterwards assert that nothing was left on that state's stack.
 * The helper is protected because only a node that is an agent action target has any use for it, and the
 * state is private because only a session may drive it, so neither is reachable from a test otherwise.
 */
class ToolDeclaringScriptNode : public ScriptNode
{
	public:

		ToolDeclaringScriptNode(const std::string& coreScript) : ScriptNode(coreScript, "") {}

		/**
		 * @note Must not be called while the caller holds a session on this node's core state; the bindings
		 *       claim one of their own.
		 */
		std::vector<Handle<ModelToolBindings>> toolBindings(
			AgenticHarness::Capability capability = AgenticHarness::Capability::LOW, unsigned serial = 1)
		{
			return _getScriptToolBindings(capability, serial);
		}

		/// The node's core state, set the first time bindings are registered against it.
		lua_State* capturedState = nullptr;

	protected:

		void _registerCoreGlobals(lua_State* luaState) override
		{
			capturedState = luaState;
			ScriptNode::_registerCoreGlobals(luaState);
		}
};

namespace
{
	/**
	 * Ask a node for the one bindings object its script declares.
	 * @param node Node to ask.
	 * @returns The bindings. Invalid if the script declared no usable tool.
	 */
	Handle<ModelToolBindings> _onlyToolBindings(ToolDeclaringScriptNode* node)
	{
		std::vector<Handle<ModelToolBindings>> tools = node -> toolBindings();

		if(tools.size() != 1) return Handle<ModelToolBindings>(0);

		return tools[0];
	}

}

/**
 * The overwhelmingly common case: a script that says nothing about tools offers none, and costs the model
 * nothing. Every scripted node written before this mechanism existed is this case.
 */
TEST(ScriptToolBindingsTest, ScriptWithNoGetToolCallBindingsOffersNoTools)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode("value = 1");

	EXPECT_TRUE(node -> toolBindings().empty());

	node -> decrRef();
}

/**
 * A descriptor is data the model is shown, so every part of it has to survive the crossing intact: the tool's
 * name and description, each parameter's name, description, type and required flag, and the return type.
 */
TEST(ScriptToolBindingsTest, DeclaredToolRoundTripsItsWholeDefinition)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function setSpin(args) return args.rate end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{\n"
		"    name = 'setSpin',\n"
		"    description = 'Set the spin rate of this node.',\n"
		"    parameters = {\n"
		"      {name = 'rate', description = 'Turns per second.', type = ToolType.NUMBER},\n"
		"      {name = 'note', description = 'Why.', type = ToolType.STRING, required = false}\n"
		"    },\n"
		"    returns = {name = 'rate', description = 'The rate now set.', type = ToolType.NUMBER}\n"
		"  }}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());
	EXPECT_TRUE(bindings.getInstance() -> hasBinding("setSpin"));
	EXPECT_FALSE(bindings.getInstance() -> hasBinding("somethingElse"));

	std::vector<ModelToolDefinition> definitions = bindings.getInstance() -> getModelToolDefinitions();

	ASSERT_EQ(definitions.size(), 1u);
	EXPECT_EQ(definitions[0].getName(), "setSpin");
	EXPECT_EQ(definitions[0].getDescription(), "Set the spin rate of this node.");

	std::vector<ModelToolDefinitionParameter> parameters = definitions[0].getParameters();

	ASSERT_EQ(parameters.size(), 2u);

	EXPECT_EQ(parameters[0].getName(), "rate");
	EXPECT_EQ(parameters[0].getDescription(), "Turns per second.");
	EXPECT_TRUE(parameters[0].getRequired()) << "A parameter saying nothing about required should be required.";
	ASSERT_TRUE(std::holds_alternative<ModelToolDefinitionParameter::PrimitiveType>(parameters[0].getType()));
	EXPECT_EQ(std::get<ModelToolDefinitionParameter::PrimitiveType>(parameters[0].getType()),
		ModelToolDefinitionParameter::PrimitiveType::NUMBER);

	EXPECT_EQ(parameters[1].getName(), "note");
	EXPECT_FALSE(parameters[1].getRequired()) << "required = false should reach the definition.";

	EXPECT_TRUE(definitions[0].hasReturnType()) << "A declared returns should reach the definition as one.";
	EXPECT_EQ(definitions[0].getReturnType().getName(), "rate");
	EXPECT_EQ(definitions[0].getReturnType().getDescription(), "The rate now set.");

	node -> decrRef();
}

/**
 * The point of the whole mechanism: the model calls the tool, the script's own global runs, and what it
 * returns comes back as the tool's result.
 */
TEST(ScriptToolBindingsTest, ToolCallRunsTheScriptGlobalAndReturnsItsValue)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"spin = 0\n"
		"function setSpin(args) spin = args.rate return spin end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{\n"
		"    name = 'setSpin', description = 'Set the spin rate.',\n"
		"    parameters = {{name = 'rate', description = 'Turns per second.', type = ToolType.NUMBER}},\n"
		"    returns = {name = 'rate', description = 'The rate now set.', type = ToolType.NUMBER}\n"
		"  }}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolCallParameterValue> values;
	values.push_back(ModelToolCallParameterValue("rate", 2.5));

	ModelToolCallParameterValue result = bindings.getInstance() -> processBinding("setSpin", values);

	EXPECT_EQ(result.getParameterName(), "rate") << "The result should be named after the declared return type.";
	ASSERT_TRUE(std::holds_alternative<double>(result.getValue()));
	EXPECT_DOUBLE_EQ(std::get<double>(result.getValue()), 2.5);

	// The call runs against the node's own persistent state, so what the tool set is still there afterwards.
	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		double spin = 0;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("spin", spin));
		EXPECT_DOUBLE_EQ(spin, 2.5) << "A tool should be able to leave state behind for the script.";
	}

	node -> decrRef();
}

/**
 * Each primitive type has to survive the crossing in both directions, and a value is read as the type the
 * script declared rather than as whatever Lua happens to be holding - which is what makes an INTEGER return
 * of 2.6 arrive as 3 rather than as the 0 lua_tointeger() would give for a non-integral number.
 */
TEST(ScriptToolBindingsTest, EveryPrimitiveTypeConvertsInBothDirections)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function echoString(args) return args.value .. '!' end\n"
		"function echoNumber(args) return args.value * 2 end\n"
		"function echoInteger(args) return args.value + 0.6 end\n"
		"function echoBool(args) return not args.value end\n"
		"local function tool(name, toolType)\n"
		"  return {name = name, description = 'Echo.',\n"
		"    parameters = {{name = 'value', description = 'In.', type = toolType}},\n"
		"    returns = {name = 'value', description = 'Out.', type = toolType}}\n"
		"end\n"
		"function getToolCallBindings(capability)\n"
		"  return {tool('echoString', ToolType.STRING), tool('echoNumber', ToolType.NUMBER),\n"
		"    tool('echoInteger', ToolType.INTEGER), tool('echoBool', ToolType.BOOL)}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());
	ASSERT_EQ(bindings.getInstance() -> getModelToolDefinitions().size(), 4u);

	std::vector<ModelToolCallParameterValue> stringValues;
	stringValues.push_back(ModelToolCallParameterValue("value", std::string("hello")));
	EXPECT_EQ(std::get<std::string>(
		bindings.getInstance() -> processBinding("echoString", stringValues).getValue()), "hello!");

	std::vector<ModelToolCallParameterValue> numberValues;
	numberValues.push_back(ModelToolCallParameterValue("value", 1.5));
	EXPECT_DOUBLE_EQ(std::get<double>(
		bindings.getInstance() -> processBinding("echoNumber", numberValues).getValue()), 3.0);

	std::vector<ModelToolCallParameterValue> integerValues;
	integerValues.push_back(ModelToolCallParameterValue("value", (long long) 2));
	EXPECT_EQ(std::get<long long>(
		bindings.getInstance() -> processBinding("echoInteger", integerValues).getValue()), 3)
		<< "An INTEGER return should round rather than collapsing a non-integral number to zero.";

	std::vector<ModelToolCallParameterValue> boolValues;
	boolValues.push_back(ModelToolCallParameterValue("value", true));
	EXPECT_FALSE(std::get<bool>(
		bindings.getInstance() -> processBinding("echoBool", boolValues).getValue()));

	node -> decrRef();
}

/**
 * An array crosses as a table indexed from 1, the same shape arrays take everywhere else in the Lua API, and
 * comes back as the vector alternative matching its declared element type.
 */
TEST(ScriptToolBindingsTest, ArrayParametersConvertInBothDirections)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function doubleAll(args)\n"
		"  local out = {}\n"
		"  for i, value in ipairs(args.values) do out[i] = value * 2 end\n"
		"  return out\n"
		"end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{\n"
		"    name = 'doubleAll', description = 'Double every value.',\n"
		"    parameters = {{name = 'values', description = 'In.', type = ToolType.NUMBER, array = true}},\n"
		"    returns = {name = 'values', description = 'Out.', type = ToolType.NUMBER, array = true}\n"
		"  }}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolDefinition> definitions = bindings.getInstance() -> getModelToolDefinitions();

	ASSERT_EQ(definitions.size(), 1u);

	std::vector<ModelToolDefinitionParameter> parameters = definitions[0].getParameters();

	ASSERT_EQ(parameters.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<ModelToolDefinitionParameter::ArrayType>(parameters[0].getType()));
	EXPECT_EQ(std::get<ModelToolDefinitionParameter::ArrayType>(parameters[0].getType()).elementType,
		ModelToolDefinitionParameter::PrimitiveType::NUMBER);

	std::vector<ModelToolCallParameterValue> values;
	values.push_back(ModelToolCallParameterValue("values", std::vector<double>{1.0, 2.0, 3.0}));

	ModelToolCallParameterValue result = bindings.getInstance() -> processBinding("doubleAll", values);

	ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.getValue()));

	std::vector<double> doubled = std::get<std::vector<double>>(result.getValue());

	ASSERT_EQ(doubled.size(), 3u);
	EXPECT_DOUBLE_EQ(doubled[0], 2.0);
	EXPECT_DOUBLE_EQ(doubled[1], 4.0);
	EXPECT_DOUBLE_EQ(doubled[2], 6.0);

	node -> decrRef();
}

/**
 * A string restricted to a list of choices is the one parameter shape that tells the model what it may send
 * rather than only what type to send, so it has to reach the definition as a choice rather than as a plain
 * string.
 */
TEST(ScriptToolBindingsTest, StringChoicesReachTheDefinition)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function setMode(args) return args.mode end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{\n"
		"    name = 'setMode', description = 'Set the mode.',\n"
		"    parameters = {{name = 'mode', description = 'Mode.', type = ToolType.STRING,\n"
		"      choices = {'fast', 'slow'}}},\n"
		"    returns = {name = 'mode', description = 'Mode now set.', type = ToolType.STRING}\n"
		"  }}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolDefinition> definitions = bindings.getInstance() -> getModelToolDefinitions();

	ASSERT_EQ(definitions.size(), 1u);

	std::vector<ModelToolDefinitionParameter> parameters = definitions[0].getParameters();

	ASSERT_EQ(parameters.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<ModelToolDefinitionParameter::StringChoice>(parameters[0].getType()));

	std::vector<std::string> choices =
		std::get<ModelToolDefinitionParameter::StringChoice>(parameters[0].getType()).stringChoices;

	ASSERT_EQ(choices.size(), 2u);
	EXPECT_EQ(choices[0], "fast");
	EXPECT_EQ(choices[1], "slow");

	node -> decrRef();
}

/**
 * A tool the model would be offered and could never call is worse than one it never sees, so a descriptor
 * naming no implementation is dropped - and dropped on its own, leaving the usable tools beside it standing.
 */
TEST(ScriptToolBindingsTest, DescriptorNamingNoGlobalFunctionIsDroppedAlone)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function realTool(args) return true end\n"
		"function getToolCallBindings(capability)\n"
		"  local function describe(name)\n"
		"    return {name = name, description = 'A tool.', parameters = {},\n"
		"      returns = {name = 'ok', description = 'Whether it worked.', type = ToolType.BOOL}}\n"
		"  end\n"
		"  return {describe('missingTool'), describe('realTool')}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolDefinition> definitions = bindings.getInstance() -> getModelToolDefinitions();

	ASSERT_EQ(definitions.size(), 1u) << "A descriptor with no implementation should be dropped.";
	EXPECT_EQ(definitions[0].getName(), "realTool");
	EXPECT_FALSE(bindings.getInstance() -> hasBinding("missingTool"));

	node -> decrRef();
}

/**
 * A descriptor leaving returns out declares a tool that does something rather than reports something. Its
 * function need not return anything at all, and the definition carries no return type of its own, so nothing
 * downstream describes a result to the model that the tool never offered. The call is still answered, since
 * no provider permits a tool result message with no content.
 */
TEST(ScriptToolBindingsTest, DescriptorWithNoReturnTypeDeclaresAResultlessTool)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"spin = 5\n"
		"function resetSpin(args) spin = 0 end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'resetSpin', description = 'Reset the spin rate to zero.', parameters = {}}}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid()) << "A descriptor with no returns should still declare a usable tool.";

	std::vector<ModelToolDefinition> definitions = bindings.getInstance() -> getModelToolDefinitions();

	ASSERT_EQ(definitions.size(), 1u);
	EXPECT_FALSE(definitions[0].hasReturnType()) << "The tool declared no result, so nor should its definition.";

	std::vector<ModelToolCallParameterValue> values;

	ModelToolCallParameterValue result = bindings.getInstance() -> processBinding("resetSpin", values);

	EXPECT_EQ(result.getParameterName(), "ok");
	ASSERT_TRUE(std::holds_alternative<bool>(result.getValue()));
	EXPECT_TRUE(std::get<bool>(result.getValue()))
		<< "A resultless tool that ran to the end should answer true.";

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		double spin = 5;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("spin", spin));
		EXPECT_DOUBLE_EQ(spin, 0.0) << "The tool should have done its work.";
	}

	node -> decrRef();
}

/**
 * A resultless tool that raises is still a failure: the boolean it is given reports that the call completed,
 * so it must never be answered true by a call that did not.
 */
TEST(ScriptToolBindingsTest, RaisingResultlessToolStillThrows)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function raisingTool(args) error('deliberate failure') end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'raisingTool', description = 'A tool.', parameters = {}}}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolCallParameterValue> values;

	try
	{
		bindings.getInstance() -> processBinding("raisingTool", values);
		FAIL() << "A resultless tool that raises should still throw.";
	}
	catch(AgentException& exception)
	{
		EXPECT_EQ(exception.getError(), AgentException::SCRIPT_TOOL_FAILED);
	}

	node -> decrRef();
}

/**
 * A returns that is there but cannot be read is a mistake in the descriptor rather than an omission from it,
 * so it is not quietly treated as a tool that declared no result.
 */
TEST(ScriptToolBindingsTest, DescriptorWithAnUnreadableReturnTypeIsDropped)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function badReturns(args) return true end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'badReturns', description = 'A tool.', parameters = {},\n"
		"    returns = {name = 'ok', description = 'Out.', type = 'not a ToolType'}}}\n"
		"end\n");

	EXPECT_TRUE(node -> toolBindings().empty());

	node -> decrRef();
}

/**
 * A descriptor whose parameter declares no valid type takes the whole tool with it rather than only that
 * parameter: dropping the parameter alone would leave the model calling the tool with arguments the script
 * never agreed to.
 */
TEST(ScriptToolBindingsTest, DescriptorWithAnUnreadableParameterIsDropped)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function badParameter(args) return true end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'badParameter', description = 'A tool.',\n"
		"    parameters = {{name = 'value', description = 'In.', type = 'not a ToolType'}},\n"
		"    returns = {name = 'ok', description = 'Out.', type = ToolType.BOOL}}}\n"
		"end\n");

	EXPECT_TRUE(node -> toolBindings().empty());

	node -> decrRef();
}

/**
 * A name these bindings do not expose is the caller's mistake rather than the script's, and is reported as
 * such: processJsonToolCall() turns the exception into something the model is told, so it must be the
 * binding-not-found description rather than a script failure.
 */
TEST(ScriptToolBindingsTest, UnknownBindingNameThrows)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function realTool(args) return true end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'realTool', description = 'A tool.', parameters = {},\n"
		"    returns = {name = 'ok', description = 'Out.', type = ToolType.BOOL}}}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolCallParameterValue> values;

	try
	{
		bindings.getInstance() -> processBinding("noSuchTool", values);
		FAIL() << "Processing a binding this object does not expose should throw.";
	}
	catch(AgentException& exception)
	{
		EXPECT_EQ(exception.getError(), AgentException::BINDING_NOT_FOUND);
	}

	node -> decrRef();
}

/**
 * A tool function that raises must not take the model request down with it. The exception carries a
 * description written to be read by the model, which is then free to correct itself and try again.
 */
TEST(ScriptToolBindingsTest, RaisingToolThrowsScriptToolFailed)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function raisingTool(args) error('deliberate failure') end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'raisingTool', description = 'A tool.', parameters = {},\n"
		"    returns = {name = 'ok', description = 'Out.', type = ToolType.BOOL}}}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolCallParameterValue> values;

	try
	{
		bindings.getInstance() -> processBinding("raisingTool", values);
		FAIL() << "A tool function that raises should throw.";
	}
	catch(AgentException& exception)
	{
		EXPECT_EQ(exception.getError(), AgentException::SCRIPT_TOOL_FAILED);
	}

	// The state has to survive a failed call intact, so the next one still works.
	std::vector<ModelToolCallParameterValue> secondValues;

	EXPECT_THROW(bindings.getInstance() -> processBinding("raisingTool", secondValues), AgentException);

	node -> decrRef();
}

/**
 * A tool returning something that is not what it declared is as unusable as one that raised, and is reported
 * the same way rather than being quietly turned into a zero or an empty string.
 */
TEST(ScriptToolBindingsTest, ToolReturningTheWrongTypeThrowsScriptToolFailed)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function wrongReturn(args) return 'not a boolean' end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'wrongReturn', description = 'A tool.', parameters = {},\n"
		"    returns = {name = 'ok', description = 'Out.', type = ToolType.BOOL}}}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolCallParameterValue> values;

	try
	{
		bindings.getInstance() -> processBinding("wrongReturn", values);
		FAIL() << "A tool returning the wrong type should throw.";
	}
	catch(AgentException& exception)
	{
		EXPECT_EQ(exception.getError(), AgentException::SCRIPT_TOOL_FAILED);
	}

	node -> decrRef();
}

/**
 * A tool that declared a result and then returned nothing has not answered its own call. lua_pcall() pads the
 * missing result with nil, and a nil is not a value of any declared type - least of all BOOL, where
 * lua_toboolean() would happily turn it into a confident false. The model is told the tool failed rather than
 * being handed an answer the script never gave. A script with nothing to report says so by leaving returns
 * out of the descriptor entirely, which is the case above.
 */
TEST(ScriptToolBindingsTest, ToolReturningNothingThrowsScriptToolFailed)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"ran = false\n"
		"function silentTool(args) ran = true end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'silentTool', description = 'A tool.', parameters = {},\n"
		"    returns = {name = 'ok', description = 'Out.', type = ToolType.BOOL}}}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolCallParameterValue> values;

	try
	{
		bindings.getInstance() -> processBinding("silentTool", values);
		FAIL() << "A tool returning nothing should throw rather than answering with a nil turned into false.";
	}
	catch(AgentException& exception)
	{
		EXPECT_EQ(exception.getError(), AgentException::SCRIPT_TOOL_FAILED);
	}

	// The tool did run, and whatever it did stands. Only its answer was missing.
	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		bool ran = false;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("ran", ran));
		EXPECT_TRUE(ran) << "The tool's own work should not be undone by its failure to return a value.";
	}

	node -> decrRef();
}

/**
 * A tool returning more than it declared keeps its first value and drops the rest, because that is what
 * lua_pcall() asked for. A script author who returns a value and an error message gets the value.
 */
TEST(ScriptToolBindingsTest, ToolReturningExtraValuesKeepsOnlyTheFirst)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function chattyTool(args) return 7, 'and something else' end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'chattyTool', description = 'A tool.', parameters = {},\n"
		"    returns = {name = 'value', description = 'Out.', type = ToolType.INTEGER}}}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid());

	std::vector<ModelToolCallParameterValue> values;

	EXPECT_EQ(std::get<long long>(
		bindings.getInstance() -> processBinding("chattyTool", values).getValue()), 7);

	node -> decrRef();
}

/**
 * The capability reaches the script, so one node can offer a weaker model a smaller set of tools than it
 * offers a stronger one.
 */
TEST(ScriptToolBindingsTest, CapabilityReachesTheScript)
{
	const std::string coreScript =
		"function basicTool(args) return true end\n"
		"function advancedTool(args) return true end\n"
		"function getToolCallBindings(capability)\n"
		"  local function describe(name)\n"
		"    return {name = name, description = 'A tool.', parameters = {},\n"
		"      returns = {name = 'ok', description = 'Out.', type = ToolType.BOOL}}\n"
		"  end\n"
		"  if capability == 'HIGH' then return {describe('basicTool'), describe('advancedTool')} end\n"
		"  return {describe('basicTool')}\n"
		"end\n";

	ToolDeclaringScriptNode* lowNode = new ToolDeclaringScriptNode(coreScript);

	std::vector<Handle<ModelToolBindings>> lowTools = lowNode -> toolBindings(AgenticHarness::Capability::LOW, 1);

	ASSERT_EQ(lowTools.size(), 1u);
	EXPECT_EQ(lowTools[0].getInstance() -> getModelToolDefinitions().size(), 1u);

	lowNode -> decrRef();

	ToolDeclaringScriptNode* highNode = new ToolDeclaringScriptNode(coreScript);

	std::vector<Handle<ModelToolBindings>> highTools =
		highNode -> toolBindings(AgenticHarness::Capability::HIGH, 1);

	ASSERT_EQ(highTools.size(), 1u);
	EXPECT_EQ(highTools[0].getInstance() -> getModelToolDefinitions().size(), 2u)
		<< "A script should be able to offer a stronger model more tools.";

	highNode -> decrRef();
}

/**
 * The serial of the action driving the request is staged for the script the same way a poke's contents are,
 * so a tool can tell one call apart from the next.
 */
TEST(ScriptToolBindingsTest, ToolCallSerialIsStagedForTheScript)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function readSerial(args) return TOOL_CALL_SERIAL end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'readSerial', description = 'A tool.', parameters = {},\n"
		"    returns = {name = 'serial', description = 'Out.', type = ToolType.INTEGER}}}\n"
		"end\n");

	std::vector<Handle<ModelToolBindings>> tools = node -> toolBindings(AgenticHarness::Capability::LOW, 42);

	ASSERT_EQ(tools.size(), 1u);

	std::vector<ModelToolCallParameterValue> values;

	EXPECT_EQ(std::get<long long>(
		tools[0].getInstance() -> processBinding("readSerial", values).getValue()), 42);

	node -> decrRef();
}

/**
 * Asking for tools is not a run: it must bring the script's globals into existence, and spend init()'s single
 * attempt if it has not been spent, without ever calling invoke(). Otherwise a model looking at a node would
 * drive that node's per-strobe work as a side effect.
 */
TEST(ScriptToolBindingsTest, AskingForToolsPrimesTheScriptWithoutCallingInvoke)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function init() initRuns = (initRuns or 0) + 1 end\n"
		"function invoke() invokeRuns = (invokeRuns or 0) + 1 end\n"
		"function aTool(args) return true end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'aTool', description = 'A tool.', parameters = {},\n"
		"    returns = {name = 'ok', description = 'Out.', type = ToolType.BOOL}}}\n"
		"end\n");

	Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

	ASSERT_TRUE(bindings.isValid()) << "A script defining init()/invoke() should still declare its tools.";

	std::vector<ModelToolCallParameterValue> values;

	bindings.getInstance() -> processBinding("aTool", values);

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		int initRuns = 0, invokeRuns = 0;

		ASSERT_TRUE(session -> getGlobal("initRuns", initRuns));
		EXPECT_EQ(initRuns, 1) << "Priming for tools should spend init()'s single attempt.";

		EXPECT_FALSE(session -> getGlobal("invokeRuns", invokeRuns))
			<< "Asking for tools, and calling one, should never run invoke().";

		// A run afterwards finds init() already spent and goes straight on to invoke().
		ASSERT_TRUE(session -> run());

		ASSERT_TRUE(session -> getGlobal("initRuns", initRuns));
		EXPECT_EQ(initRuns, 1) << "init() should still have been called exactly once.";

		ASSERT_TRUE(session -> getGlobal("invokeRuns", invokeRuns));
		EXPECT_EQ(invokeRuns, 1);
	}

	node -> decrRef();
}

/**
 * The core state outlives the node's every run and every tool call, so anything left on its stack accumulates
 * for the life of the hive. Every path through reading the descriptors and calling a tool, including the ones
 * that fail, has to leave the stack as it found it.
 */
TEST(ScriptToolBindingsTest, ReadingAndCallingToolsLeavesTheCoreStackClean)
{
	ToolDeclaringScriptNode* node = new ToolDeclaringScriptNode(
		"function goodTool(args) return true end\n"
		"function raisingTool(args) error('deliberate failure') end\n"
		"function getToolCallBindings(capability)\n"
		"  local function describe(name)\n"
		"    return {name = name, description = 'A tool.',\n"
		"      parameters = {{name = 'value', description = 'In.', type = ToolType.STRING}},\n"
		"      returns = {name = 'ok', description = 'Out.', type = ToolType.BOOL}}\n"
		"  end\n"
		"  return {describe('goodTool'), describe('raisingTool'), describe('missingTool')}\n"
		"end\n");

	for(int i = 0; i < 3; i++)
	{
		Handle<ModelToolBindings> bindings = _onlyToolBindings(node);

		ASSERT_TRUE(bindings.isValid());

		std::vector<ModelToolCallParameterValue> values;
		values.push_back(ModelToolCallParameterValue("value", std::string("x")));

		bindings.getInstance() -> processBinding("goodTool", values);

		EXPECT_THROW(bindings.getInstance() -> processBinding("raisingTool", values), AgentException);
	}

	ASSERT_NE(node -> capturedState, nullptr);
	EXPECT_EQ(lua_gettop(node -> capturedState), 0)
		<< "Reading descriptors and calling tools should leave nothing behind on the core stack.";

	node -> decrRef();
}

namespace
{
	/// The core script used by the merge tests below, declaring one tool of its own.
	const char* const _SCRIPT_TOOL_SOURCE =
		"function scriptTool(args) return true end\n"
		"function getToolCallBindings(capability)\n"
		"  return {{name = 'scriptTool', description = 'A tool the script declared.', parameters = {},\n"
		"    returns = {name = 'ok', description = 'Out.', type = ToolType.BOOL}}}\n"
		"end\n";

	/**
	 * Collect the names of every tool a set of bindings exposes.
	 * @param tools Bindings to read.
	 * @returns The tool names, in the order the bindings report them.
	 */
	std::vector<std::string> _toolNames(std::vector<Handle<ModelToolBindings>> tools)
	{
		std::vector<std::string> names;

		for(Handle<ModelToolBindings>& toolHandle : tools)
		{
			for(ModelToolDefinition& definition : toolHandle.getInstance() -> getModelToolDefinitions())
			{
				names.push_back(definition.getName());
			}
		}

		return names;
	}

	/**
	 * Whether a name appears among a set of tool names.
	 */
	bool _holdsTool(std::vector<std::string> names, const std::string& name)
	{
		for(const std::string& held : names)
		{
			if(held == name) return true;
		}

		return false;
	}
}

/**
 * The tools a node's script declares are offered on top of the fixed set its node type already offers, not
 * instead of them: a triggerNode running such a script still gives the model emitTrigger.
 */
TEST(ScriptToolBindingsTest, ScriptToolsAreAddedToTheNodeTypesOwnTools)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ModelToolBindingsFactory* factory = new ModelToolBindingsFactory();
	Handle<GraphToolBindingsFactory> factoryHandle(factory);

	// The handle holds the reference; release the implicit construction ref.
	factory -> decrRef();

	hive -> setToolBindingsFactory(factoryHandle);

	TriggerNode* node = new TriggerNode(_SCRIPT_TOOL_SOURCE, "");

	hive -> addNode(node);

	std::vector<std::string> names = _toolNames(
		node -> getModelToolBindings(AgenticHarness::Capability::LOW, 1));

	EXPECT_TRUE(_holdsTool(names, "emitTrigger")) << "The node type's own tools should still be offered.";
	EXPECT_TRUE(_holdsTool(names, "scriptTool")) << "The script's tools should be offered alongside them.";

	hive -> shutdown();
}

/**
 * The factory only holds what a node type offers alike, so a hive with none set has no fixed tools to give.
 * A tool a script declared is the node's own and does not come from there, so it survives that.
 */
TEST(ScriptToolBindingsTest, ScriptToolsSurviveAHiveWithNoToolBindingsFactory)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	TriggerNode* node = new TriggerNode(_SCRIPT_TOOL_SOURCE, "");

	hive -> addNode(node);

	std::vector<std::string> names = _toolNames(
		node -> getModelToolBindings(AgenticHarness::Capability::LOW, 1));

	EXPECT_FALSE(_holdsTool(names, "emitTrigger")) << "No factory means none of the node type's own tools.";
	EXPECT_TRUE(_holdsTool(names, "scriptTool")) << "A script defined tool should not depend on a factory.";

	hive -> shutdown();
}

/**
 * AnimateScriptNode leaves StrobeActionTarget::strobe() abstract, so the merge has to be exercised through a
 * concrete subclass. Declared here rather than borrowed from another suite so this one does not depend on the
 * order the test headers are included in.
 */
class ToolDeclaringAnimateScriptNode : public AnimateScriptNode
{
	public:

		ToolDeclaringAnimateScriptNode(const std::string& coreScript)
			: AnimateScriptNode(coreScript, "") {}

		void strobe() override {}
};

/**
 * The merge is the same on the other node class that is an agent action target, and so on the scene script
 * nodes that inherit it: a script's tools arrive alongside the animating tools rather than displacing them.
 */
TEST(ScriptToolBindingsTest, ScriptToolsAreAddedToAnAnimateScriptNodesOwnTools)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ModelToolBindingsFactory* factory = new ModelToolBindingsFactory();
	Handle<GraphToolBindingsFactory> factoryHandle(factory);

	// The handle holds the reference; release the implicit construction ref.
	factory -> decrRef();

	hive -> setToolBindingsFactory(factoryHandle);

	ToolDeclaringAnimateScriptNode* node = new ToolDeclaringAnimateScriptNode(_SCRIPT_TOOL_SOURCE);

	hive -> addNode(node);

	std::vector<std::string> names = _toolNames(
		node -> getModelToolBindings(AgenticHarness::Capability::LOW, 1));

	EXPECT_TRUE(_holdsTool(names, "getAnimating")) << "The node type's own tools should still be offered.";
	EXPECT_TRUE(_holdsTool(names, "setAnimating"));
	EXPECT_TRUE(_holdsTool(names, "scriptTool")) << "The script's tools should be offered alongside them.";

	hive -> shutdown();
}

/**
 * A node whose script declares no tools is left exactly as it was before this mechanism existed, which is
 * what every scripted node already in a hive relies on.
 */
TEST(ScriptToolBindingsTest, NodeWithNoScriptToolsOffersOnlyItsNodeTypesTools)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ModelToolBindingsFactory* factory = new ModelToolBindingsFactory();
	Handle<GraphToolBindingsFactory> factoryHandle(factory);

	// The handle holds the reference; release the implicit construction ref.
	factory -> decrRef();

	hive -> setToolBindingsFactory(factoryHandle);

	TriggerNode* node = new TriggerNode("value = 1", "");

	hive -> addNode(node);

	std::vector<std::string> names = _toolNames(
		node -> getModelToolBindings(AgenticHarness::Capability::LOW, 1));

	ASSERT_EQ(names.size(), 1u);
	EXPECT_EQ(names[0], "emitTrigger");

	hive -> shutdown();
}

#endif
