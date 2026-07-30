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
every concrete node type that has a `scriptSource` (currently `SceneGeometryScriptNode` and
`SceneTransformScriptNode`), in both the core and poke states.

| Function | Description |
|---|---|
| `getStrobe()` | Returns `true` if this node is currently marked as strobing, `false` otherwise. |
| `getAnimating()` | Returns `true` if this node is currently in animating mode, `false` otherwise. |
| `setAnimating(animating, [emitAnimateAction])` | Sets whether this node is in animating mode. `animating` is a boolean. `emitAnimateAction` is an optional boolean (default `false`); if `true` and the mode actually changed, an `AnimateAction` is emitted from this node. |
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
`NodeType.SCENE_TRANSFORM_SCRIPT_NODE`, `NodeType.SCENE_ROOT_NODE` and `NodeType.TELEPORT_NODE`. Note that
`NodeType.GRAPH_NODE` is a type in its own right (reported by any node that does not identify a more
specific one), not a wildcard; to place no restriction on type, leave the argument out.

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

A node only *receives* a trigger if it is a trigger target. No concrete node type is one yet, so
`trigger()` currently emits an action that traverses the graph without visibly affecting any of the node
types documented below.

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
| `VertexVisibility` | A global table of constants naming when a vertex is visible: `VertexVisibility.ALWAYS` (always visible), `VertexVisibility.GRABBED` (visible while grabbed, e.g. mouse button held down), `VertexVisibility.DRAGGING` (visible while being dragged, e.g. mouse down then move) and `VertexVisibility.HOVERED_OVER` (visible while hovered over, e.g. a non-button mouse over). Pass one of these as the optional `visibility` argument to `addVertex`/`addVertexes`. |
| `addVertex(vertex, [visibility])` | Appends a single `Vertex` (as built by the `Vertex` constructor) to this node's vertex list. `visibility` is an optional `VertexVisibility.*` constant (default `VertexVisibility.ALWAYS`); an unrecognized value raises an error. |
| `addVertexes(vertexes, [visibility])` | Appends every `Vertex` in the given array-style table (indexes `1..#vertexes`) to this node's vertex list in one call. `visibility` is an optional `VertexVisibility.*` constant (default `VertexVisibility.ALWAYS`); an unrecognized value raises an error. |
| `vertexCount()` | Returns the number of vertexes currently held by this node. |

Vertexes are appended in the order added; each consecutive triplet defines a triangle with
counter-clockwise winding order for the front face, matching the `vertexes` array in the JSON schema.

## `sceneTransformNode`

No `scriptSource`; its `transform` is set directly from JSON rather than by a script.

## `sceneTransformScriptNode`

Represents a transform applied to scene geometry, which `coreScript` can read and modify. In addition to
the [common bindings](#bindings-common-to-all-lua-scripted-nodes) above:

| Function | Description |
|---|---|
| `getTransform()` | Returns the node's current transform as a 16-element array table, in column-major order, matching the `transform` array in the JSON schema. Translation components are in scene geometry units, which by default map one unit to one millimeter. |
| `setTransform(transform)` | Sets the node's transform from a 16-element array table, in column-major order. Translation components are in scene geometry units, which by default map one unit to one millimeter. |
