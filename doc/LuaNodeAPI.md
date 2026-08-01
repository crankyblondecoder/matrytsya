# Lua Node API

This document describes the Lua bindings available to a hive's `coreScript`/`pokeScript` sources, on a
per concrete node type basis. The concrete node types are those listed under `#/$defs/node` in
`hiveSchema.json`. Only node types with a `scriptSource` (`coreScript` + `pokeScript`) run Lua at all;
the others are listed for completeness and have no Lua surface.

Each Lua-scripted node owns two separate, sandboxed Lua states: one that `coreScript` runs against
(invoked each time the node fires), and one that `pokeScript` runs against (invoked each time the node is
poked, only relevant if `pokeEnabled` is set). Bindings described as "core" or "poke" below are only
callable from the corresponding script; bindings with no such qualifier are registered into both states
and callable from either.

## Sandboxing

Every Lua state a script runs in, whether core or poke, is sandboxed identically:

- Only the base, coroutine, math, string, table and utf8 standard libraries are opened. `io`, `os`,
  `package` and `debug` are never opened, so scripts have no filesystem, process, environment or
  introspection access.
- `dofile`, `loadfile`, `print` and `warn` are removed from the base library.
- The global `load` is replaced with a version that only accepts source text, never precompiled bytecode.
- Memory is capped independently per state (core and poke are counted separately) at 1 MiB. An allocation
  that would exceed the cap fails.
- A state's global environment persists across invocations: a global a script sets on one run is still
  visible as the starting state on the next run of that same script on that same node instance. This lets
  a script keep state (e.g. a counter or direction flag) in an ordinary global.
- Each node's state is its own. Nothing a script sets is visible to another node's script, and the core
  and poke states of a single node are equally isolated from each other. State crosses between nodes only
  when a `ScriptAction` carries it: such an action can publish globals into each node's environment just
  before that node's script runs, and can read back what a script left behind. The names involved are
  chosen by whichever action subclass does the publishing, so they are not listed here.

## Bindings common to all Lua-scripted nodes

These are registered by `ScriptNode`, `StrobeScriptNode` and `AnimateScriptNode`, and so are available to
every concrete node type that has a `scriptSource` (currently `sceneGeometryScriptNode`,
`sceneTransformScriptNode` and `triggerNode`), in both the core and poke states.

| Function | Description |
|---|---|
| `getStrobe()` | Returns `true` if this node is currently marked as strobing, `false` otherwise. |
| `getAnimating()` | Returns `true` if this node is currently in animating mode, `false` otherwise. |
| `setAnimating(animating, [emitAnimateAction])` | Sets whether this node is in animating mode. `animating` is a boolean. `emitAnimateAction` is an optional boolean (default `false`); if `true`, an `AnimateAction` is emitted from this node, carrying the mode to every node that action reaches. It is emitted even when this node was already in the given mode, so a mode can be re-asserted over a subtree in which a node has since cleared itself (for example a transform script that stops itself once its animation has run down). |
| `NodeType` | A global table of constants naming each concrete node type, for use as the `nodeType` argument to `trigger`. See [Triggering other nodes](#triggering-other-nodes). |
| `trigger([nodeName], [nodeType])` | Emits a `TriggerAction` from this node, triggering nodes it reaches as it traverses the graph. See [Triggering other nodes](#triggering-other-nodes). |

### Triggering other nodes

`trigger()` emits a `TriggerAction` from the node whose script called it. That action traverses the graph
outward from this node and fires every node it reaches that is a trigger target and that passes the
restrictions below.

Both arguments are optional, and either may be `nil` to skip that restriction:

| Argument | Effect |
|---|---|
| `nodeName` | Only nodes whose name matches this string are triggered. Omitted, `nil` or `""` means any name. |
| `nodeType` | Only nodes of this type are triggered. Must be one of the `NodeType` constants; any other value raises an error rather than silently triggering everything. Omitted or `nil` means any type. |

The `NodeType` constants are `NodeType.GRAPH_NODE`, `NodeType.PING_NODE`, `NodeType.SCENE_GEOMETRY_NODE`,
`NodeType.SCENE_TRANSFORM_NODE`, `NodeType.SCRIPT_NODE`, `NodeType.SCENE_GEOMETRY_SCRIPT_NODE`,
`NodeType.SCENE_TRANSFORM_SCRIPT_NODE`, `NodeType.SCENE_ROOT_NODE`, `NodeType.TELEPORT_NODE`,
`NodeType.AGENT_NODE` and `NodeType.TRIGGER_NODE`. Note that `NodeType.GRAPH_NODE` is a type in its own
right (reported by any node that does not identify a more specific one), not a wildcard; to place no
restriction on type, leave the argument out.

```lua
-- Every trigger target reached.
trigger()

-- Only nodes named "leftDoor".
trigger("leftDoor")

-- Only scene geometry script nodes, whatever they are named.
trigger(nil, NodeType.SCENE_GEOMETRY_SCRIPT_NODE)

-- Only a scene transform script node named "leftDoor".
trigger("leftDoor", NodeType.SCENE_TRANSFORM_SCRIPT_NODE)
```

The emitted action is never applied to the node that emitted it, so a script cannot trigger the node it is
running on. The call returns immediately: the action traverses the graph independently of the script run
that emitted it, so nothing it triggers is guaranteed to have happened by the time the next line of the
script executes.

`trigger()` is registered into both the core and the poke state, so a node can fire a subgraph either when
it is invoked or in response to being poked.

A node only *receives* a trigger if it is a trigger target. Of the node types documented below, only
`agentNode` is one: a trigger reaching it makes it emit its prompts as an agent action, unless its
`autoTriggerAgentAction` is set false. Every other node type ignores a trigger, so an unrestricted
`trigger()` fires the agent nodes the action reaches and passes over everything else.

Note that `triggerNode` *emits* triggers rather than receiving them; it is not a trigger target.

### Poke-only context globals

The following globals are set immediately before `pokeScript` runs, describing the poke that triggered it.
They are only meaningful (and only set) in the poke state, not the core state:

| Global | Type | Description |
|---|---|---|
| `POKE_TYPE` | string | One of `"HIT"`, `"GRAB"`, `"DRAG"`, `"HOVER_ENTER"`, `"HOVER_LEAVE"`. A `pokeScript` runs for every poke type, so a script that should only react to a click (rather than the pointer hovering on/off it) must guard on `POKE_TYPE == "HIT"`. |
| `HIT_DURATION` | integer | Hit duration in milliseconds. Only meaningful when `POKE_TYPE == "HIT"`. |
| `DRAG_VECTOR` | array table of 3 numbers | Drag vector in world coordinates, `{x, y, z}`. Only meaningful when `POKE_TYPE == "DRAG"`. |

## `pingNode`

No `scriptSource`; this node type never runs Lua.

## `teleportNode`

No `scriptSource`; this node type never runs Lua.

## `sceneRootNode`

No `scriptSource`; this node type never runs Lua.

## `sceneGeometryNode`

No `scriptSource`; its `vertexes` are set directly from JSON rather than by a script.

## `sceneGeometryScriptNode`

Represents scene geometry whose vertexes are populated by `coreScript` (typically once, though it may add
more on later invocations). In addition to the [common bindings](#bindings-common-to-all-lua-scripted-nodes)
above:

| Function | Description |
|---|---|
| `Vertex{posn = {...}, colour = {...}, texCoords = {...}, normal = {...}}` | Constructs a `Vertex` userdata. All fields are optional; any field left out is zeroed. `posn` and `normal` are 3-element arrays of numbers (X, Y, Z); `posn` is in scene geometry units, which by default map one unit to one millimeter. `colour` is a 4-element array of integers 0-255 (R, G, B, A). `texCoords` is a 2-element array of numbers (U, V). |
| `VertexVisibility` | A global table of constants naming when a vertex is visible: `VertexVisibility.ALWAYS` (always visible), `VertexVisibility.AGENT` (visible only while this node's agent visible flag is set, see `setAgentVisible`), `VertexVisibility.GRABBED` (visible while grabbed, e.g. mouse button held down), `VertexVisibility.DRAGGING` (visible while being dragged, e.g. mouse down then move) and `VertexVisibility.HOVERED_OVER` (visible while hovered over, e.g. a non-button mouse over). Pass one of these as the optional `visibility` argument to `addVertex`/`addVertexes`. |
| `addVertex(vertex, [visibility])` | Appends a single `Vertex` (as built by the `Vertex` constructor) to this node's vertex list. `visibility` is an optional `VertexVisibility.*` constant (default `VertexVisibility.ALWAYS`); an unrecognized value raises an error. |
| `addVertexes(vertexes, [visibility])` | Appends every `Vertex` in the given array-style table (indexes `1..#vertexes`) to this node's vertex list in one call. `visibility` is an optional `VertexVisibility.*` constant (default `VertexVisibility.ALWAYS`); an unrecognized value raises an error. |
| `vertexCount()` | Returns the number of vertexes currently held by this node. |
| `setAgentVisible(visible)` | Sets whether this node's `VertexVisibility.AGENT` vertexes are currently shown. `visible` is a boolean. This is also set automatically for the duration of an agentic request made against this node, so a script only needs it to drive the same geometry for reasons of its own. |
| `getAgentVisible()` | Returns `true` if this node's `VertexVisibility.AGENT` vertexes are currently shown, `false` otherwise. |

Vertexes are appended in the order added; each consecutive triplet defines a triangle with
counter-clockwise winding order for the front face, matching the `vertexes` array in the JSON schema.

`VertexVisibility.AGENT` geometry is the one visibility the server decides. Setting the flag costs a single
node id in the data the viewer polls for; the vertexes themselves are never resent, so it is cheap enough to
toggle as often as wanted.

## `sceneTransformNode`

No `scriptSource`; its `transform` is set directly from JSON rather than by a script.

## `sceneTransformScriptNode`

Represents a transform applied to scene geometry, which `coreScript` can read and modify. In addition to
the [common bindings](#bindings-common-to-all-lua-scripted-nodes) above:

| Function | Description |
|---|---|
| `getTransform()` | Returns the node's current transform as a 16-element array table, in column-major order, matching the `transform` array in the JSON schema. Translation components are in scene geometry units, which by default map one unit to one millimeter. |
| `setTransform(transform)` | Sets the node's transform from a 16-element array table, in column-major order. Translation components are in scene geometry units, which by default map one unit to one millimeter. |

## `agentNode`

No `scriptSource`; this node type never runs Lua. It is a trigger target: a trigger reaching it emits its
prompts as an agent action, unless its `autoTriggerAgentAction` is set false. See
[Triggering other nodes](#triggering-other-nodes).

## `triggerNode`

Has a `scriptSource`, but adds no Lua bindings of its own beyond the
[common bindings](#bindings-common-to-all-lua-scripted-nodes) above, so its `coreScript` can call
`trigger()` exactly as any other scripted node can.

What distinguishes it is that the same emission is exposed to an AI model rather than only to Lua. While an
agent action is being applied to this node, the model servicing it is offered an `emitTrigger` tool taking
the same two optional restrictions `trigger()` takes, `nodeName` and `nodeType`, with `nodeType` restricted
to the same set of names as the `NodeType` constants. Calling it emits a trigger action from this node, so
the model decides which part of the graph fires next. As with `trigger()`, the emitted action is never
applied back to this node.

`coreScript` and `pokeScript` run exactly as they do on any other scripted node, with the
[poke-only context globals](#poke-only-context-globals) set as usual. On top of that, poking the node emits
an unrestricted trigger action of its own once `pokeScript` has returned, unless `emitTriggerOnPoke` is set
false. The script runs first either way, so anything it leaves behind is already in place before that
trigger reaches the rest of the graph.
