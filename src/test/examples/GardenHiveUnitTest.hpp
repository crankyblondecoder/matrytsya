#ifndef GARDEN_HIVE_UNIT_TEST_H
#define GARDEN_HIVE_UNIT_TEST_H

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphPoke.hpp"
#include "../../graph/nodes/AnimateScriptNode.hpp"
#include "../../persist/HiveBuilder.hpp"
#include "../../persist/json/JsonHiveLoader.hpp"
#include "../../rapidjson/document.h"
#include "../../rapidjson/stringbuffer.h"
#include "../../rapidjson/writer.h"
#include "../../util/Handle.hpp"

namespace
{
	/**
	 * Read examples/gardenHive.json and strip its surfaces and strobe registrations, keeping every node and edge
	 * exactly as shipped.
	 * @returns The hive JSON, or an empty string if the file could not be read or parsed.
	 * @note The path is found from this header's own location rather than from the working directory alone.
	 *       That location is relative to where the test binary was compiled, which is also where it is run.
	 * @note A strobe fires every 40ms for as long as the hive lives, so with strobing left in place there is
	 *       never a moment when no action is active to wait for. A strobe also runs each transform's core
	 *       script, which may clear the transform's own animating flag, so the flags read afterwards would
	 *       depend on timing.
	 */
	std::string __loadGardenHiveWithoutStrobing()
	{
		std::string thisFile = __FILE__;
		std::string::size_type srcTest = thisFile.rfind("src/test/");

		if(srcTest == std::string::npos) return "";

		std::ifstream file(thisFile.substr(0, srcTest) + "examples/gardenHive.json");

		if(!file) return "";

		std::stringstream contents;
		contents << file.rdbuf();

		rapidjson::Document document;
		document.Parse(contents.str().c_str());

		if(document.HasParseError() || !document.IsObject()) return "";

		document.RemoveMember("surfaces");
		document.RemoveMember("strobeEmitters");
		document.RemoveMember("strobeSurfaces");

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		document.Accept(writer);

		return buffer.GetString();
	}

	/**
	 * Get whether the named node in a hive is animating.
	 * @param hive Hive to find the node in.
	 * @param nodeName Name of a node that supports animation.
	 * @returns Whether the node is animating. False, with a test failure recorded, if no such animating node exists.
	 */
	bool __isAnimating(GraphHive* hive, const std::string& nodeName)
	{
		Handle<GraphNode> handle = hive -> getNode(nodeName);

		AnimateScriptNode* node = handle.isValid() ? dynamic_cast<AnimateScriptNode*>(handle.getInstance()) : nullptr;

		EXPECT_NE(node, nullptr) << "No animating node named " << nodeName;

		return node ? node -> getAnimating() : false;
	}
}

/**
 * Hitting the button in the shipped garden hive animates all three flowers and the button itself, and hitting it
 * again stops all of them.
 *
 * An action never forks, so a single animate action can only follow one of the button's edges to a flower and
 * dies at the end of that flower. Instead the button emits one animate action per flower, each holding the key of
 * the edge to its flower, plus an unkeyed one that can only take the route round the button.
 */
TEST(GardenHiveTest, ButtonAnimatesEveryFlower)
{
	std::string json = __loadGardenHiveWithoutStrobing();
	ASSERT_FALSE(json.empty()) << "Could not load examples/gardenHive.json.";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	// For each flower: its body, which the button's keyed edge reaches first; its transforms that animate; and
	// its last petal, showing that the action went all the way through the flower. buttonLift is on the
	// unkeyed action's route round the button.
	const char* const watched[] = {"body1", "tilt1_6", "petal6", "body2", "spinTransform2", "petal6_2", "body3",
		"spin3_6", "petal6_3", "buttonCap", "buttonLift"};

	for(const char* name : watched)
	{
		EXPECT_FALSE(__isAnimating(hive, name)) << name << " should not be animating before the button is hit.";
	}

	Handle<GraphNode> button = hive -> getNode("buttonCap");
	ASSERT_TRUE(button.isValid());

	GraphPoke::PokeData data{};
	data.hitDuration = 100;

	button.getInstance() -> poke(GraphPoke(GraphPoke::PokeType::HIT, data));
	hive -> waitOnNoActionsActive(0);

	for(const char* name : watched)
	{
		EXPECT_TRUE(__isAnimating(hive, name)) << name << " should be animating after the first hit.";
	}

	button.getInstance() -> poke(GraphPoke(GraphPoke::PokeType::HIT, data));
	hive -> waitOnNoActionsActive(0);

	for(const char* name : watched)
	{
		EXPECT_FALSE(__isAnimating(hive, name)) << name << " should have stopped after the second hit.";
	}

	hive -> shutdown();
}

#endif
