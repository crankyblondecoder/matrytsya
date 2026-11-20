#ifndef API_DOCS_UNIT_TEST_H
#define API_DOCS_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../api/ApiController.hpp"
#include "../../api/apiDocs.hpp"
#include "../../graph/GraphNode.hpp"
#include "../../persist/json/JsonHiveLoader.hpp"
#include "../../rapidjson/document.h"
#include "../../rapidjson/stringbuffer.h"
#include "../../rapidjson/writer.h"

#include <algorithm>
#include <ctype.h>
#include <set>
#include <string>
#include <vector>

namespace
{
	rapidjson::Document _parseDoc(const char* text)
	{
		rapidjson::Document document;

		document.Parse(text);

		return document;
	}

	std::string _upper(std::string value)
	{
		for(char& character : value) character = (char) toupper((unsigned char) character);

		return value;
	}
}

/**
 * Every document served is valid JSON.
 */
TEST(ApiDocsTest, ServedDocumentsParse)
{
	EXPECT_FALSE(_parseDoc(apiDocs::OPENAPI_SPEC).HasParseError());
	EXPECT_FALSE(_parseDoc(apiDocs::HIVE_SCHEMA).HasParseError());
	EXPECT_GT(std::string(apiDocs::AGENT_GUIDE).size(), 0u);
}

/**
 * The OpenAPI description documents exactly the routes the API serves: none missing and none extra.
 */
TEST(ApiDocsTest, OpenApiMatchesRoutes)
{
	rapidjson::Document spec = _parseDoc(apiDocs::OPENAPI_SPEC);

	ASSERT_TRUE(spec.IsObject());
	ASSERT_TRUE(spec.HasMember("paths"));

	static const std::set<std::string> METHODS = {"get", "put", "post", "delete", "patch", "head", "options"};

	std::set<std::string> documented;

	for(rapidjson::Value::MemberIterator path = spec["paths"].MemberBegin(); path != spec["paths"].MemberEnd(); path++)
	{
		for(rapidjson::Value::MemberIterator operation = path -> value.MemberBegin();
			operation != path -> value.MemberEnd(); operation++)
		{
			std::string method = operation -> name.GetString();

			if(METHODS.count(method)) documented.insert(_upper(method) + " " + path -> name.GetString());
		}
	}

	std::set<std::string> served;

	for(ApiController::RouteDescription& route : ApiController::getRoutes())
	{
		served.insert(route.method + " " + route.pattern);
	}

	for(const std::string& route : served) EXPECT_TRUE(documented.count(route)) << route << " is not documented.";
	for(const std::string& route : documented) EXPECT_TRUE(served.count(route)) << route << " is not served.";

	// The agent guide lists every operation too.
	std::string guide = apiDocs::AGENT_GUIDE;

	for(ApiController::RouteDescription& route : ApiController::getRoutes())
	{
		EXPECT_NE(guide.find("| " + route.method + " | `" + route.pattern + "` |"), std::string::npos)
			<< route.method << " " << route.pattern << " is missing from the agent guide's operation list.";
	}
}

/**
 * The node types a trigger can be restricted to are documented as the server names them.
 */
TEST(ApiDocsTest, TriggerNodeTypesMatch)
{
	rapidjson::Document spec = _parseDoc(apiDocs::OPENAPI_SPEC);

	rapidjson::Value& types = spec["components"]["schemas"]["Trigger"]["properties"]["nodeType"]["enum"];

	std::vector<std::string> documented;

	for(rapidjson::Value& type : types.GetArray()) documented.push_back(type.GetString());

	std::vector<std::string> served = GraphNode::typeNames();

	std::sort(documented.begin(), documented.end());
	std::sort(served.begin(), served.end());

	EXPECT_EQ(documented, served);
}

/**
 * The documented example bodies for creating a hive and a node are accepted by the loaders the API parses them with.
 */
TEST(ApiDocsTest, ExamplesAreValid)
{
	rapidjson::Document spec = _parseDoc(apiDocs::OPENAPI_SPEC);

	auto example = [&spec](const char* path, const char* method) -> std::string
	{
		rapidjson::Value& value = spec["paths"][path][method]["requestBody"]["content"]["application/json"]["example"];

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		value.Accept(writer);

		return std::string(buffer.GetString(), buffer.GetSize());
	};

	EXPECT_NO_THROW({ JsonHiveLoader loader(example("/hives", "post")); });
	EXPECT_NO_THROW(JsonHiveLoader::parseNodeFragment(example("/hives/{hive}/nodes", "post")));
}

#endif
