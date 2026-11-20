#ifndef API_DOCS_H
#define API_DOCS_H

/**
 * Documentation of the hive control API, served by the API itself so that a client can discover it from the
 * server alone.
 * @note Generated into apiDocs.cpp from the files in doc/ by build/gen_api_docs.sh, which the build runs. Edit the
 *       files in doc/, never apiDocs.cpp.
 */
namespace apiDocs
{
	/// OpenAPI 3.1 description of the API, from doc/hiveApi.openapi.json.
	extern const char* const OPENAPI_SPEC;

	/// Guide to driving the API, written for an AI agent, from doc/hiveApiAgentGuide.md.
	extern const char* const AGENT_GUIDE;

	/// JSON Schema of a hive definition, from doc/hiveSchema.json, which the OpenAPI description refers to.
	extern const char* const HIVE_SCHEMA;
}

#endif
