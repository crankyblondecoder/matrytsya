#ifndef BUILDER_SCRIPT_NODE_H
#define BUILDER_SCRIPT_NODE_H

#include <cstddef>
#include <string>
#include <vector>

#include "../actionTargets/AgentActionTarget.hpp"
#include "../actionTargets/TriggerActionTarget.hpp"
#include "../../thread/ThreadMutex.hpp"
#include "../../util/Handle.hpp"
#include "ScriptNode.hpp"

class GraphNode;
class ModelToolBindings;

struct lua_State;

/**
 * Scripted graph node whose scripts can grow the hive it is part of, creating new nodes and the edges between
 * them while the hive is running.
 *
 * Each concrete node type a builder can create has a binding of its own, createPingNode{...},
 * createSceneTextScriptNode{...} and so on, taking one table of named fields that mirror that type's fields in a
 * hive definition. createEdge(fromName, toName, [options]) then wires any two nodes in the hive together, so
 * a script can hang what it creates off nodes that were already there as well as the reverse. The hive lookup
 * bindings getHiveName(), getSelfName(), getNodeNames(), nodeExists(), getNodeInfo() and getNodesOfType() let a
 * script find those existing nodes. The ActionFlag and VertexVisibility constant tables are registered alongside
 * them, to fill the fields that take one.
 *
 * Its core script runs when a trigger action reaches it, as well as under a script action, so building can be
 * set off by another script's trigger(), by poking a TriggerNode, by a model's emitTrigger tool or from another
 * hive through a TeleportNode. Otherwise it behaves as a TriggerNode does: a poke runs the poke script, and an
 * agent action offers the model the hive's node tools plus any the script declares. It takes no part in a scene.
 *
 * @note A builder cannot create another builder, so a script cannot set off construction that goes on
 *       building past what it wrote itself.
 * @note With buildOnce set, which it is by default, the node builds once only: the first script session, core or
 *       poke, to call a create binding or createEdge() is the build, and once it ends every later call to one of
 *       them returns false and a reason rather than building the same nodes and edges again. A call that raises
 *       for a malformed argument does not start the build. The lookup bindings remain usable throughout.
 * @note A malformed argument raises a Lua error, and nothing is created. A call whose arguments were fine but
 *       that could not be carried out against the hive as it stands - a name already in use, an endpoint that
 *       does not exist, a node with no edge slots left, a builder not in a hive - returns false and a reason
 *       instead, so a script can react to the hive having changed under it.
 */
class BuilderScriptNode : public ScriptNode, public AgentActionTarget, public TriggerActionTarget
{
    public:

		/**
		 * @param coreScript Main Lua source code that this node runs when invoked.
		 * @param pokeScript The script that is called for processing a poke.
		 * @param buildOnce If true, only the first script session that builds anything may build; later ones
		 *        are refused. If false, every session may build.
		 * @param built If true, and buildOnce is too, the one build is taken as already used up, as it is for a
		 *        builder restored from a hive that already holds what it built. Ignored if buildOnce is false.
		 */
        BuilderScriptNode(const std::string& coreScript, const std::string& pokeScript, bool buildOnce = true,
			bool built = false);

		GraphNodeType getType() override;

		virtual void accept(GraphNodeVisitor& visitor) override;

		/**
		 * Get whether this node builds once only.
		 */
		bool getBuildOnce();

		/**
		 * Get whether this node's one build has been used up.
		 * @returns True once the session that built has ended. Always false if this node does not build once only.
		 */
		bool getBuilt();

		// Agent target API point.
		std::vector<Handle<ModelToolBindings>> getModelToolBindings(AgenticHarness::Capability capability,
			unsigned serial) override;

		AgentActionTarget* getAgentActionTarget() override;

		/**
		 * Run the core script, in a core session of its own, as a trigger action reaching this node asks.
		 * @note A trigger that cannot claim the core state is dropped rather than propagated back into the graph.
		 */
		void trigger() override;

		TriggerActionTarget* getTriggerActionTarget() override;

	protected:

		// Ref counted.
        virtual ~BuilderScriptNode();

		virtual void _registerCoreGlobals(lua_State* luaState) override;

		virtual void _sessionEnding(bool poke) override;

	private:

		/**
		 * How far through its one build a node that builds once only has got.
		 */
		enum class BuildState
		{
			/// Nothing has been built yet.
			NOT_BUILT,
			/// A session is building. It may go on building until it ends.
			BUILDING,
			/// The session that built has ended. Nothing more may be built.
			BUILT
		};

		/**
		 * Concrete node types a builder can create, one create binding apiece. Exposed to Lua only through the
		 * binding each is registered under, never as a value.
		 */
		enum class CreatableNodeType
		{
			PING,
			TELEPORT,
			SCENE_ROOT,
			SCENE_GEOMETRY,
			SCENE_GEOMETRY_SCRIPT,
			SCENE_TRANSFORM,
			SCENE_TRANSFORM_SCRIPT,
			SCENE_TEXT_SCRIPT,
			AGENT,
			TRIGGER,
			SCENE_IMAGE
		};

		/**
		 * Outcome of carrying out a binding, decided before anything is handed back to Lua.
		 */
		enum class BuildStatus
		{
			/// Carried out.
			OK,
			/// An argument was malformed. Raised as a Lua error.
			ARG_ERROR,
			/// The arguments were fine but the hive could not take what was asked. Returned as false and a reason.
			RUNTIME_ERROR
		};

		/**
		 * Everything a create binding read out of its fields table. Fields that do not apply to the type being
		 * created are left at their defaults.
		 */
		struct NodeSpec;

        // Do not allow copying.
        BuilderScriptNode(const BuilderScriptNode& copyFrom);
        BuilderScriptNode& operator= (const BuilderScriptNode& copyFrom);

		/**
		 * Register the constant tables, create bindings, createEdge() and hive lookup bindings into luaState,
		 * binding each function to this node instance via upvalue.
		 * @param luaState Lua state to register into.
		 */
		void __registerBuilderBindings(lua_State* luaState);

		/**
		 * Lua binding behind every create...Node{fields}. Upvalue 1 is this node, upvalue 2 the
		 * CreatableNodeType it creates, upvalue 3 whether it was registered into the poke state.
		 * @returns true on success, or false and a reason if the hive could not take the node.
		 * @throw Raises a Lua error if the fields table is malformed.
		 */
		static int __luaCreateNode(lua_State* luaState);

		/**
		 * Lua binding: createEdge(fromName, toName, [{actionFlags, traversalKey, actionsCompleteAfterTraverse}]).
		 * Creates an edge directed from one node in this builder's hive to another. Upvalue 1 is this node, upvalue
		 * 2 whether it was registered into the poke state.
		 * @returns true on success, or false and a reason if either node is missing or the edge could not be made.
		 * @throw Raises a Lua error if an argument is malformed.
		 */
		static int __luaCreateEdge(lua_State* luaState);

		/**
		 * Lua binding: getHiveName(). Returns the name of this builder's hive, or nil if it is in none.
		 */
		static int __luaGetHiveName(lua_State* luaState);

		/**
		 * Lua binding: getSelfName(). Returns this builder's own name.
		 */
		static int __luaGetSelfName(lua_State* luaState);

		/**
		 * Lua binding: getNodeNames(). Returns an array of the name of every node in this builder's hive.
		 */
		static int __luaGetNodeNames(lua_State* luaState);

		/**
		 * Lua binding: nodeExists(name). Returns whether a node of that name is in this builder's hive.
		 * @throw Raises a Lua error if name is not a string.
		 */
		static int __luaNodeExists(lua_State* luaState);

		/**
		 * Lua binding: getNodeInfo(name). Returns {id, name, type, edges = {{toNodeName, toNodeId, actionFlags},
		 * ...}} for the named node in this builder's hive, or nil if there is none. type is a NodeType constant
		 * and actionFlags an array of ActionFlag constants.
		 * @throw Raises a Lua error if name is not a string.
		 */
		static int __luaGetNodeInfo(lua_State* luaState);

		/**
		 * Lua binding: getNodesOfType(nodeType). Returns an array of the name of every node of that type in this
		 * builder's hive.
		 * @throw Raises a Lua error if nodeType is not a NodeType constant.
		 */
		static int __luaGetNodesOfType(lua_State* luaState);

		/**
		 * Hand a binding's outcome back to Lua: true, false and the reason, or a raised error.
		 * @param luaState Lua state the binding is running against.
		 * @param status Outcome of the binding.
		 * @param error Reason, for any status other than OK.
		 * @returns Number of values left on the stack. Does not return for ARG_ERROR.
		 * @note Only to be called once nothing C++ is left alive in the binding's frame, as a raised error skips
		 *       every destructor between here and the Lua call.
		 */
		static int __finishBinding(lua_State* luaState, BuildStatus status, const char* error);

		/**
		 * Carry out a create binding: read and validate the fields table, then create the node and add it to
		 * this builder's hive.
		 * @param luaState Lua state the binding is running against; argument 1 is the fields table.
		 * @param type Type of node to create.
		 * @param poke Whether the binding is running in the poke state.
		 * @param error Set to the reason for any status other than OK. Must be a buffer of the size every binding here reports into.
		 * @returns The outcome.
		 * @note Never raises a Lua error, and never lets a C++ exception escape, so every object it holds is
		 *       destroyed before its caller reports the outcome.
		 */
		BuildStatus __createNode(lua_State* luaState, CreatableNodeType type, bool poke, char* error);

		/**
		 * Carry out createEdge(), validating every argument before anything is looked up.
		 * @param luaState Lua state the binding is running against.
		 * @param poke Whether the binding is running in the poke state.
		 * @param error Set to the reason for any status other than OK. Must be a buffer of the size every binding here reports into.
		 * @returns The outcome.
		 * @note Never raises a Lua error, and never lets a C++ exception escape.
		 */
		BuildStatus __createEdge(lua_State* luaState, bool poke, char* error);

		/**
		 * Claim the right to build for the session currently running in the given state.
		 * @param poke Whether that session is on the poke state.
		 * @returns True if this node does not build once only, if nothing has been built yet (in which case
		 *          that session becomes the one that builds), or if that session is already the one building.
		 *          False if the build is used up, or another state's session is building.
		 */
		bool __claimBuild(bool poke);

		/**
		 * Read a create binding's fields table into a spec, checking every field before any is used.
		 * @param luaState Lua state to read from.
		 * @param index Absolute stack index of the fields table.
		 * @param type Type of node the fields describe, which decides which fields it takes.
		 * @param spec Filled in from the fields.
		 * @param error Set to the reason if a field is malformed. Must be a buffer of the size every binding here reports into.
		 * @returns True if every field was read.
		 * @note Never raises a Lua error.
		 */
		static bool __readNodeSpec(lua_State* luaState, int index, CreatableNodeType type, NodeSpec& spec,
			char* error);

		/**
		 * Construct the node a spec describes, with every type specific field applied to it.
		 * @param type Type of node to construct.
		 * @param spec Fields to construct it from.
		 * @returns Newly allocated node, carrying its initial reference.
		 * @throw Whatever the node's constructor or setters throw. A node already constructed is released first.
		 */
		static GraphNode* __constructNode(CreatableNodeType type, const NodeSpec& spec);

		/// Every CreatableNodeType, in the same order as CREATE_BINDING_NAMES.
		static constexpr CreatableNodeType CREATABLE_NODE_TYPES[] = {
			CreatableNodeType::PING,
			CreatableNodeType::TELEPORT,
			CreatableNodeType::SCENE_ROOT,
			CreatableNodeType::SCENE_GEOMETRY,
			CreatableNodeType::SCENE_GEOMETRY_SCRIPT,
			CreatableNodeType::SCENE_TRANSFORM,
			CreatableNodeType::SCENE_TRANSFORM_SCRIPT,
			CreatableNodeType::SCENE_TEXT_SCRIPT,
			CreatableNodeType::AGENT,
			CreatableNodeType::TRIGGER,
			CreatableNodeType::SCENE_IMAGE};

		/// Name of the Lua global each CreatableNodeType's create binding is registered under.
		static constexpr const char* CREATE_BINDING_NAMES[] = {
			"createPingNode",
			"createTeleportNode",
			"createSceneRootNode",
			"createSceneGeometryNode",
			"createSceneGeometryScriptNode",
			"createSceneTransformNode",
			"createSceneTransformScriptNode",
			"createSceneTextScriptNode",
			"createAgentNode",
			"createTriggerNode",
			"createSceneImageNode"};

		/// Whether only the first session that builds may build.
		const bool _buildOnce;

		/// How far through its one build this node has got. Only moves on while _buildOnce is set.
		BuildState _buildState = BuildState::NOT_BUILT;

		/// Whether the session building is on the poke state. Only meaningful while _buildState is BUILDING.
		bool _buildingInPoke = false;

		/// Guards _buildState and _buildingInPoke, which the core and poke states' sessions share.
		ThreadMutex _buildLock;
};

#endif
