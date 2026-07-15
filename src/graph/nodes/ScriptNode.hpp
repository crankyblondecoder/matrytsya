#ifndef SCRIPT_NODE_H
#define SCRIPT_NODE_H

#include <cstddef>
#include <string>

#include "../actionTargets/ScriptActionTarget.hpp"
#include "../GraphNode.hpp"

struct lua_State;

/**
 * Graph node that runs a script against a Lua state provided to it when invoked.
 */
class ScriptNode : public GraphNode, public ScriptActionTarget
{
    public:

        virtual ~ScriptNode();

		/**
		 * @param coreScript Main Lua source code that this node runs when invoked.
		 * @param pokeScript The script that is called for processing a poke.
		 */
        ScriptNode(const std::string& coreScript, const std::string& pokeScript);

		bool invoke(lua_State* luaState) override;

		ScriptActionTarget* getScriptActionTarget() override;

	protected:

		/**
		 * Read an optional array field out of the table at the given stack index into a fixed-size double
		 * array, leaving entries at their existing values if the field is absent.
		 * @param luaState Lua state to read from.
		 * @param tableIndex Stack index of the table to read the field from.
		 * @param field Name of the field to read.
		 * @param out Array to write the values into.
		 * @param count Number of elements to read.
		 */
		static void _readDoubleArray(lua_State* luaState, int tableIndex, const char* field, double* out, int count);

		/**
		 * Read an optional array field out of the table at the given stack index into a fixed-size byte
		 * array, leaving entries at their existing values if the field is absent.
		 * @param luaState Lua state to read from.
		 * @param tableIndex Stack index of the table to read the field from.
		 * @param field Name of the field to read.
		 * @param out Array to write the values into.
		 * @param count Number of elements to read.
		 */
		static void _readByteArray(lua_State* luaState, int tableIndex, const char* field, std::byte* out, int count);

		void _poked(GraphPoke poke) override;

    private:

        // Do not allow copying.
        ScriptNode(const ScriptNode& copyFrom);
        ScriptNode& operator= (const ScriptNode& copyFrom);

		/**
		 * Compile _coreScript once and cache the result in _coreBytecode, so invoke() never has to
		 * re-parse the source text. _coreScript never changes after construction, so this only needs
		 * to run once.
		 */
		void __compileCoreScript();

		/**
		 * lua_Writer callback passed to lua_dump(); appends each chunk of bytecode it produces to the
		 * std::string pointed to by userData.
		 * @param luaState Lua state performing the dump.
		 * @param data Chunk of bytecode to append.
		 * @param size Number of bytes in data.
		 * @param userData Pointer to the std::string being written to.
		 * @returns 0, as required by lua_Writer to continue the dump.
		 */
		static int __writeBytecode(lua_State* luaState, const void* data, size_t size, void* userData);

		/// Main Lua source that is run each time this node is invoked.
		std::string _coreScript;

		/// Lua source that is exclusively for processing pokes.
		std::string _pokeScript;

		/// Precompiled bytecode of _coreScript, cached once at construction; empty if _coreScript failed
		/// to compile.
		std::string _coreBytecode;
};

#endif
