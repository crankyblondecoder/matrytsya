# Node Types

A catalogue of the concrete node types a hive can hold: what each one is for, what it accepts in a hive
definition, which actions it reacts to, what it emits, and what it offers Lua scripts, models and pokes. The
concrete types are exactly those listed under `#/$defs/node` in [hiveSchema.json](hiveSchema.json), and each is
named here by the string its `type` field takes.

This document gathers in one place what the other references say per type. They remain the authority on their
own subject:

- [hiveSchema.json](hiveSchema.json) — every field, its type, default and constraints.
- [matrytsya_ai.md](matrytsya_ai.md) — how actions traverse, what each action type does, strobing and limits.
- [LuaNodeAPI.md](LuaNodeAPI.md) — the Lua bindings in full, and the `coreScript` lifecycle.
- [modelTools.md](modelTools.md) — the tools a model is offered, their arguments and their returns.

## At a glance

| Type | Purpose | Runs Lua | Actions applied | Model tools | Reacts to a poke |
|---|---|---|---|---|---|
| `PingNode` | A ping point | No | Ping | — | No |
| `TeleportNode` | Forwards serialisable actions to another node, possibly in another hive | No | Ping, Trigger (as serialisable) | — | No |
| `SceneRootNode` | Roots a scene; the only strobe emitter | No | None | — | No |
| `SceneGeometryNode` | Geometry set in JSON | No | Scene, Strobe, Agent affect | — | No |
| `SceneGeometryScriptNode` | Geometry built by a script | Yes | Scene, Strobe, Script, Animate, Agent, Agent affect | `getAnimating`, `setAnimating`, plus script defined | Runs `pokeScript` |
| `SceneTransformNode` | Transform set in JSON | No | Scene, Strobe, Agent affect | — | No |
| `SceneTransformScriptNode` | Transform a script reads and sets | Yes | Scene, Strobe, Script, Animate, Agent, Agent affect | `getAnimating`, `setAnimating`, plus script defined | Runs `pokeScript` |
| `AgentNode` | Holds prompts and emits them as an agent action | No | Trigger | — | No |
| `TriggerNode` | Emits trigger actions for a script, a model or a poke | Yes | Script, Agent | `emitTrigger`, plus script defined | Runs `pokeScript`, then emits a trigger |

"Actions applied" is which action types have any effect on arriving at the node. Every node is still passed
through by actions it does not react to, and still spends their energy; see
[Action types](matrytsya_ai.md#action-types).

Pokes reach a node through a scene surface, and only for geometry that node contributes to the scene being
shown, so in practice the poke column matters for the two geometry types above all. A node with `pokeEnabled`
left false discards every poke, whatever its type.

## Fields every node carries

From `nodeBase`:

| Field | Required | Meaning |
|---|---|---|
| `type` | Yes | One of the type names in this document. |
| `name` | Yes | Unique within the hive's `nodes` array. Edges, `strobeEmitters` and surfaces refer to a node by it. |
| `pokeEnabled` | No, default `false` | Whether the node accepts pokes at all. |
| `edges` | No | Outgoing edges, at most 32. Each names a `toNodeName`, optionally restricts the actions that may cross it with `actionFlags`, may set `actionsCompleteAfterTraverse`, and may lock the edge to actions holding a matching non-zero `traversalKey`. |

Scripted types (`SceneGeometryScriptNode`, `SceneTransformScriptNode`, `TriggerNode`) also require
`coreScript` and `pokeScript`, from `scriptSource`. Either may be an empty string.

## Fields and support by type

### `PingNode`

The simplest node: a point a ping action can reach. It carries only the common fields.

- **Actions applied:** Ping, which counts the ping on this node. A ping is also serialisable, but only a
  `TeleportNode` does anything with that.
- **Emits:** Nothing of its own accord.
- **Lua, model tools, poke:** None. A poke is accepted if `pokeEnabled` is set, and does nothing.

### `TeleportNode`

Forwards each serialisable action it receives to another node, which may be in another hive of the same
collection or on another host. Ping and trigger actions are the serialisable ones.

| Field | Required | Meaning |
|---|---|---|
| `destination` | Yes | A node location: `hiveName` and `nodeName`, plus `hostname` and `port` for a hive hosted elsewhere. |

- **Actions applied:** Any action carrying `SERIALISABLE_GRAPH_ACTION`: ping and trigger. A copy of the action
  is sent to `destination` and carries on traversing the hive it lands in, while the original carries on from
  here as normal. A copy that cannot be delivered is lost without error.
- **Restricted triggers:** A trigger carrying a `nodeName` or `nodeType` restriction that this node does not
  match is passed over and never forwarded. A trigger meant to leave the hive must be unrestricted or must
  name this node.
- **Emits:** Nothing of its own accord.
- **Lua, model tools, poke:** None.

### `SceneRootNode`

The root of a scene, and the start of the pathway a scene surface collects from. It carries only the common
fields.

- **Actions applied:** None. It is a source of actions, not a target of them.
- **Emits:** A strobe action every `periodMs` when registered in `strobeEmitters`. It is the only type that
  can be registered there. A `GraphHiveSceneSurface` names it as `sceneRootNodeName`, and the scene actions
  that build that surface start from it.
- **Lua, model tools, poke:** None.

### `SceneGeometryNode`

A piece of scene geometry whose vertexes are written directly in JSON.

| Field | Required | Meaning |
|---|---|---|
| `vertexes` | No | A flat list of vertexes, each with an optional `visibility`. |
| `vertexGroups` | No | Groups of vertexes, each stating one `visibility`. Use this or `vertexes`, never both. |
| `emitAgentAffectAction` | No, default `false` | Accepted, but has no effect on this type: see below. |

Each vertex holds `posn` (required), and optionally `colour`, `texCoords` and `normal`. Every three vertexes
make one triangle, wound counter-clockwise for the front face.

- **Actions applied:** Scene, which places the geometry under the transform currently on top of the stack.
  Strobe, which does nothing here since there is no script. Agent affect, which shows or hides this node's
  `AGENT` visibility vertexes.
- **Agent visibility:** No agent action is ever applied to this type, so its `AGENT` vertexes are shown only
  when an agent affect action arrives from a scripted node upstream. For the same reason nothing ever directly
  affects it, so `emitAgentAffectAction` never sends anything on.
- **Emits:** Nothing.
- **Lua, model tools:** None.
- **Poke:** Can be poked through a scene surface, but does nothing with it.

### `SceneGeometryScriptNode`

A piece of scene geometry whose vertexes a script adds. It takes the same geometry fields as
`SceneGeometryNode`, and both kinds of vertex can be combined on one node.

| Field | Required | Meaning |
|---|---|---|
| `coreScript`, `pokeScript` | Yes | The node's two Lua scripts. |
| `vertexes` or `vertexGroups` | No | Geometry set in JSON, as on `SceneGeometryNode`. |
| `emitAgentAffectAction` | No, default `false` | When an agent action starts or stops being applied to this node, send an agent affect action onward so downstream nodes react to it too. |

- **Actions applied:** Scene, as on `SceneGeometryNode`. Strobe, which marks the node as strobing and runs
  `coreScript`. Script, which runs `coreScript` without marking it as strobing. Animate, which sets the
  node's animating mode. Agent, which sends the matching prompt to a model, with this node's tools on offer,
  and shows this node's `AGENT` vertexes for the length of the request. Agent affect, which shows or hides
  those same vertexes.
- **Emits:** An animate action from `setAnimating()` when asked to, always from the `setAnimating` model
  tool, and from `emitAction()`. A trigger action from `trigger()` or `emitAction()`. An agent affect action
  while an agent action is applied here, if `emitAgentAffectAction` is set.
- **Lua:** The bindings on every scripted node (`NodeType`, `trigger()`, `ToolType`, `ActionType`,
  `emitAction()`), the scene script node bindings (`getStrobe()`, `getAnimating()`, `setAnimating()`), and
  its own: `Vertex`, `VertexVisibility`,
  `addVertex()`, `addVertexes()`, `vertexCount()`, `setAgentVisible()`, `getAgentVisible()`. Vertexes are only
  ever added, never cleared, so fixed geometry belongs in `init()`. See
  [LuaNodeAPI.md](LuaNodeAPI.md#scenegeometryscriptnode).
- **Model tools:** `getAnimating` and `setAnimating`, plus any the script declares through
  `getToolCallBindings()`.
- **Poke:** Can be poked through a scene surface; runs `pokeScript` with the poke-only globals set.

### `SceneTransformNode`

A transform, written directly in JSON, applied to the geometry visited after it.

| Field | Required | Meaning |
|---|---|---|
| `transform` | No | A 4×4 matrix as 16 numbers in column-major order. |
| `emitAgentAffectAction` | No, default `false` | Accepted, but has no effect on this type: no agent action is applied to it, so nothing directly affects it. |

- **Actions applied:** Scene, which pushes this transform, composed with the one above it, onto the stack.
  Strobe, which does nothing here. Agent affect, which has no visible effect on a transform.
- **Emits:** Nothing.
- **Lua, model tools, poke:** None. It has no geometry of its own, so a scene surface never pokes it.

Transforms compose parent × local, and a revisit re-pushes the result of the first visit rather than
recomputing it. See [Transform behaviour during scene generation](matrytsya_ai.md#transform-behaviour-during-scene-generation).

### `SceneTransformScriptNode`

A transform that a script can read and set.

| Field | Required | Meaning |
|---|---|---|
| `coreScript`, `pokeScript` | Yes | The node's two Lua scripts. |
| `transform` | No | The starting transform, as on `SceneTransformNode`. |
| `emitAgentAffectAction` | No, default `false` | When an agent action starts or stops being applied to this node, send an agent affect action onward so downstream geometry reacts to it. |

- **Actions applied:** Scene, as on `SceneTransformNode`. Strobe, Script, Animate and Agent, as on
  `SceneGeometryScriptNode`. Agent affect, which has no visible effect on a transform.
- **Emits:** As `SceneGeometryScriptNode`: animate, trigger, and agent affect when `emitAgentAffectAction` is
  set.
- **Lua:** The bindings on every scripted node, the scene script node bindings, and its own `getTransform()`
  and `setTransform()`. See [LuaNodeAPI.md](LuaNodeAPI.md#scenetransformscriptnode).
- **Model tools:** `getAnimating` and `setAnimating`, plus any the script declares.
- **Poke:** Has a `pokeScript`, but owns no geometry, so a scene surface never pokes it.

A scripted transform runs its script again on every revisit, so a node used as a point to return to an earlier
frame should be a `SceneTransformNode`, not this type.

### `AgentNode`

Holds a list of prompts and, when triggered, emits them as one agent action. That action carries a single
conversation from node to node, so later prompts see what earlier ones produced.

| Field | Required | Meaning |
|---|---|---|
| `capability` | Yes | `LOW`, `MEDIUM` or `HIGH`. Every prompt is requested for the `NODE` role at this capability, so a harness must assign a model to that exact pair. |
| `prompts` | Yes | At least one entry: `nodeType` (required), `prompt` (required), and optionally `nodeIdentifier` and `terminateOnResponse`. At each node the action reaches, the first entry whose type, and name if given, matches supplies the prompt. A node matching no entry is passed over. |
| `autoTriggerAgentAction` | No, default `true` | Whether a trigger reaching this node emits the agent action. |
| `serialiseEmittedActions` | No, default `true` | Whether only one emitted action may be running at a time, later ones waiting their turn. If false, each trigger starts its own conversation alongside any already running. |

- **Actions applied:** Trigger. It is the only trigger target, so this is the only type a trigger fires.
- **Emits:** An agent action when triggered, unless `autoTriggerAgentAction` is false.
- **Where the agent action has effect:** Only `SceneGeometryScriptNode`, `SceneTransformScriptNode` and
  `TriggerNode` react to an agent action. A prompt written for any other type is never sent. An agent action
  never crosses an edge without `actionFlags`, so every route it takes must list `AGENT_GRAPH_ACTION`.
- **Lua, model tools, poke:** None.

### `TriggerNode`

A scripted node whose job is to emit trigger actions on behalf of a script, a model or a poke. It takes no part
in a scene.

| Field | Required | Meaning |
|---|---|---|
| `coreScript`, `pokeScript` | Yes | The node's two Lua scripts. Often both empty when emission is left to a model or a poke. |
| `emitTriggerOnPoke` | No, default `true` | Whether a poke emits an unrestricted trigger action once `pokeScript` has run. |

- **Actions applied:** Script, which runs `coreScript`. Agent, which sends the matching prompt with
  `emitTrigger` on offer. It supports neither strobe flag, so a strobe never runs its script, and an `init()`
  runs on the first script action to reach it, not at hive start.
- **Emits:** A trigger action from `trigger()` or `emitAction()`, from the `emitTrigger` model tool, and on a
  poke unless `emitTriggerOnPoke` is false. An animate action from `emitAction()`. Neither is ever applied back
  to this node.
- **Lua:** Only the bindings on every scripted node: `NodeType`, `trigger()`, `ToolType`, `ActionType` and
  `emitAction()`. None of the scene script node bindings exist here. See
  [LuaNodeAPI.md](LuaNodeAPI.md#triggernode).
- **Model tools:** `emitTrigger`, plus any the script declares.
- **Poke:** Runs `pokeScript`, then emits a trigger unless `emitTriggerOnPoke` is false. It owns no geometry,
  so a scene surface never pokes it.

## Identifying a type at runtime

A node type has two other spellings besides its `type` name. Prompts' `nodeType` field, the Lua `NodeType`
constants and the `nodeType` argument of `emitTrigger` all use the upper-case form:

| `type` | `nodeType` / `NodeType` |
|---|---|
| `PingNode` | `PING_NODE` |
| `TeleportNode` | `TELEPORT_NODE` |
| `SceneRootNode` | `SCENE_ROOT_NODE` |
| `SceneGeometryNode` | `SCENE_GEOMETRY_NODE` |
| `SceneGeometryScriptNode` | `SCENE_GEOMETRY_SCRIPT_NODE` |
| `SceneTransformNode` | `SCENE_TRANSFORM_NODE` |
| `SceneTransformScriptNode` | `SCENE_TRANSFORM_SCRIPT_NODE` |
| `AgentNode` | `AGENT_NODE` |
| `TriggerNode` | `TRIGGER_NODE` |

`GRAPH_NODE` and `SCRIPT_NODE` are also listed in that enum, but no concrete type a hive can hold reports
either of them.
