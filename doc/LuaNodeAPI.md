# Lua Node API

This document describes the Lua bindings available to a hive's `coreScript`/`pokeScript` sources, on a
per concrete node type basis. The concrete node types are those listed under `#/$defs/node` in
`hiveSchema.json`. Only node types with a `scriptSource` (`coreScript` + `pokeScript`) run Lua at all;
the others are listed for completeness and have no Lua surface.

Each Lua-scripted node owns two separate, sandboxed Lua states: one that `coreScript` runs against, and one
that `pokeScript` runs against. See [When a script runs](#when-a-script-runs) for what invokes each. Bindings
described as "core" or "poke" below are only callable from the corresponding script; bindings with no such
qualifier are registered into both states and callable from either.

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
  when a script action carries it: such an action can publish globals into each node's environment just
  before that node's script runs, and can read back what a script left behind. The names involved are
  chosen by whichever kind of action does the publishing, so they are not listed here.

## When a script runs

`coreScript` runs when a script action or a strobe action is applied to the node, and at no other time. Scene
building, animate and agent actions never run it. In practice that means a scene script node's `coreScript`
runs once per strobe of the emitter that reaches it, for as long as the hive lives. A `triggerNode` is the
exception: a strobe never reaches it, so its `coreScript` runs only under a script action.

**A `coreScript` is therefore run repeatedly and must be written to be run repeatedly.** Nothing it did on an
earlier run is undone before the next one: appended vertexes stay appended, and a transform it set stays set.
Work that should happen once has to guard itself, either on a global the script sets on its first run or on
state the node already exposes — `vertexCount() == 0` is the usual guard for geometry that is built once.
Work that should happen per frame goes outside that guard, typically under `getStrobe()`.

`pokeScript` runs on each poke, only if `pokeEnabled` is set, and never as part of traversal. Poking a node
does not run its `coreScript`, and an action reaching a node does not run its `pokeScript`.

## Bindings on every Lua-scripted node

Available to `sceneGeometryScriptNode`, `sceneTransformScriptNode` and `triggerNode`, in both the core and
poke states.

| Function | Description |
|---|---|
| `NodeType` | A global table of constants naming each concrete node type, for use as the `nodeType` argument to `trigger`. See [Triggering other nodes](#triggering-other-nodes). |
| `trigger([nodeName], [nodeType])` | Emits a trigger action from this node, triggering nodes it reaches as it traverses the graph. See [Triggering other nodes](#triggering-other-nodes). |

## Bindings on scene script nodes

Available to `sceneGeometryScriptNode` and `sceneTransformScriptNode` only, in both the core and poke states.
**These are not registered on `triggerNode`**, which takes no part in a scene and has neither a strobing nor
an animating mode; calling one of them from a `triggerNode` script raises an error on a nil value.

| Function | Description |
|---|---|
| `getStrobe()` | Returns `true` if this node is currently marked as strobing, `false` otherwise. Only a strobe action marks a node, so this is what separates a run driven by the strobe that animates the scene from a run driven by a plain script action. |
| `getAnimating()` | Returns `true` if this node is currently in animating mode, `false` otherwise. |
| `setAnimating(animating, [emitAnimateAction])` | Sets whether this node is in animating mode. `animating` is a boolean. `emitAnimateAction` is an optional boolean (default `false`); if `true`, an animate action is emitted from this node, carrying the mode to every node that action reaches. It is emitted even when this node was already in the given mode, so a mode can be re-asserted over a subtree in which a node has since cleared itself (for example a transform script that stops itself once its animation has run down). The `setAnimating` tool a model is offered at this node is not the same call: it always emits, as described in [modelTools.md](modelTools.md). |

### Triggering other nodes

`trigger()` emits a trigger action from the node whose script called it. That action traverses the graph
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

A trigger action does two things at the nodes it reaches, and both obey the restrictions above:

- **Triggering.** A node is triggered only if it is a trigger target. Of the node types documented below,
  only `agentNode` is one: a trigger reaching it makes it emit its prompts as an agent action, unless its
  `autoTriggerAgentAction` is set false. No other node type is triggered by one.
- **Teleporting.** A trigger action is also a serialisable action, so a `teleportNode` it reaches forwards it
  to that node's `destination`, where it carries on traversing the hive it lands in. This is how a trigger
  crosses from one hive to another.

Note that `triggerNode` *emits* triggers rather than receiving them; it is not a trigger target.

A restriction suppresses both. A trigger carrying a `nodeName` or `nodeType` that a `teleportNode` does not
match is passed over by it and never forwarded, exactly as an unmatched `agentNode` is never triggered. A
trigger intended to leave the hive must therefore either carry no restriction at all or name the teleport node
itself — restricting it to the agent node waiting on the far side stops it at the gate.

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

Represents scene geometry whose vertexes are populated by `coreScript`. Vertexes are appended and never
cleared, so a script that builds a fixed piece of geometry must guard the build — `if vertexCount() == 0 then`
— or every strobe adds another copy of it. In addition to the bindings
[on every Lua-scripted node](#bindings-on-every-lua-scripted-node) and
[on scene script nodes](#bindings-on-scene-script-nodes):

| Function | Description |
|---|---|
| `Vertex{posn = {...}, colour = {...}, texCoords = {...}, normal = {...}}` | Constructs a `Vertex` userdata. All fields are optional; any field left out is zeroed. `posn` and `normal` are 3-element arrays of numbers (X, Y, Z); `posn` is in scene geometry units, which by default map one unit to one millimeter. `colour` is a 4-element array of numbers 0-255 (R, G, B, A), each rounded to the nearest integer, so a value computed by interpolating between two colours can be passed as it stands. `texCoords` is a 2-element array of numbers (U, V). |
| `VertexVisibility` | A global table of constants naming when a vertex is visible: `VertexVisibility.ALWAYS` (always visible), `VertexVisibility.AGENT` (visible only while this node's agent visible flag is set, see `setAgentVisible`), `VertexVisibility.GRABBED` (visible while grabbed, e.g. mouse button held down), `VertexVisibility.DRAGGING` (visible while being dragged, e.g. mouse down then move) and `VertexVisibility.HOVERED_OVER` (visible while hovered over, e.g. a non-button mouse over). Pass one of these as the optional `visibility` argument to `addVertex`/`addVertexes`. |
| `addVertex(vertex, [visibility])` | Appends a single `Vertex` (as built by the `Vertex` constructor) to this node's vertex list. `visibility` is an optional `VertexVisibility.*` constant (default `VertexVisibility.ALWAYS`); an unrecognized value raises an error. |
| `addVertexes(vertexes, [visibility])` | Appends every `Vertex` in the given array-style table (indexes `1..#vertexes`) to this node's vertex list in one call. `visibility` is an optional `VertexVisibility.*` constant (default `VertexVisibility.ALWAYS`); an unrecognized value raises an error. |
| `vertexCount()` | Returns the number of vertexes currently held by this node. |
| `setAgentVisible(visible)` | Sets whether this node's `VertexVisibility.AGENT` vertexes are currently shown. `visible` is a boolean. This is also set automatically for the duration of an agentic request made against this node, so a script only needs it to drive the same geometry for reasons of its own. |
| `getAgentVisible()` | Returns `true` if this node's `VertexVisibility.AGENT` vertexes are currently shown, `false` otherwise. |

Vertexes are appended in the order added; each consecutive triplet defines a triangle with
counter-clockwise winding order for the front face, matching the `vertexes` array in the JSON schema. The two
routes differ on colour alone: the JSON array takes integers only, while the `Vertex` constructor rounds
whatever number it is given.

`VertexVisibility.AGENT` geometry is the one visibility the server decides. Setting the flag costs a single
node id in the data the viewer polls for; the vertexes themselves are never resent, so it is cheap enough to
toggle as often as wanted.

## `sceneTransformNode`

No `scriptSource`; its `transform` is set directly from JSON rather than by a script.

## `sceneTransformScriptNode`

Represents a transform applied to scene geometry, which `coreScript` can read and modify. In addition to the
bindings [on every Lua-scripted node](#bindings-on-every-lua-scripted-node) and
[on scene script nodes](#bindings-on-scene-script-nodes):

| Function | Description |
|---|---|
| `getTransform()` | Returns the node's current transform as a 16-element array table, in column-major order, matching the `transform` array in the JSON schema. Translation components are in scene geometry units, which by default map one unit to one millimeter. |
| `setTransform(transform)` | Sets the node's transform from a 16-element array table, in column-major order. Translation components are in scene geometry units, which by default map one unit to one millimeter. |

## `agentNode`

No `scriptSource`; this node type never runs Lua. It is a trigger target: a trigger reaching it emits its
prompts as an agent action, unless its `autoTriggerAgentAction` is set false. See
[Triggering other nodes](#triggering-other-nodes).

## `triggerNode`

Has a `scriptSource`, but adds no Lua bindings of its own. It takes no part in a scene, so it has only the
[bindings on every Lua-scripted node](#bindings-on-every-lua-scripted-node) — `NodeType` and `trigger()` —
and none of the [scene script node bindings](#bindings-on-scene-script-nodes): `getStrobe()`,
`getAnimating()` and `setAnimating()` do not exist here.

It also supports neither strobe flag, so a strobe never reaches its script. Its `coreScript` runs only under
a script action, which is why a `triggerNode` whose emission is left to a model or to a poke usually has an
empty `coreScript`.

What distinguishes it is that the same emission is exposed to an AI model rather than only to Lua. While an
agent action is being applied to this node, the model servicing it is offered an `emitTrigger` tool taking
the same two optional restrictions `trigger()` takes, `nodeName` and `nodeType`, with `nodeType` restricted
to the same set of names as the `NodeType` constants. Calling it emits a trigger action from this node, so
the model decides which part of the graph fires next. As with `trigger()`, the emitted action is never
applied back to this node. See [modelTools.md](modelTools.md) for the tool's exact arguments and return.

`coreScript` and `pokeScript` run exactly as they do on any other scripted node, with the
[poke-only context globals](#poke-only-context-globals) set as usual. On top of that, poking the node emits
an unrestricted trigger action of its own once `pokeScript` has returned, unless `emitTriggerOnPoke` is set
false. The script runs first either way, so anything it leaves behind is already in place before that
trigger reaches the rest of the graph.
