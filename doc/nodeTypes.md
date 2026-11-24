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
| `SceneTextScriptNode` | Text in a scene, which a script reads and sets | Yes | Scene, Strobe, Script, Animate, Agent | `getAnimating`, `setAnimating`, plus script defined | Runs `pokeScript` |
| `SceneImageNode` | An image in a scene: embedded, a local file or a URL | No | Scene, Image visible | — | No |
| `AgentNode` | Holds prompts and emits them as an agent action | No | Trigger | — | No |
| `TriggerNode` | Emits trigger actions for a script, a model or a poke | Yes | Script, Agent | `emitTrigger`, plus script defined | Runs `pokeScript`, then emits a trigger |
| `BuilderScriptNode` | Creates nodes and edges in its hive from Lua | Yes | Trigger, Script, Agent | `emitTrigger`, plus script defined | Runs `pokeScript` |

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

Scripted types (`SceneGeometryScriptNode`, `SceneTransformScriptNode`, `SceneTextScriptNode`, `TriggerNode`,
`BuilderScriptNode`) also require
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
  tool, and from `emitAction()`. A trigger action from `trigger()` or `emitAction()`. A text visible or text
  change action from `emitAction()`. An agent affect action while an agent action is applied here, if
  `emitAgentAffectAction` is set.
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
- **Emits:** As `SceneGeometryScriptNode`: animate, trigger, text visible, and agent affect when
  `emitAgentAffectAction` is set.
- **Lua:** The bindings on every scripted node, the scene script node bindings, and its own `getTransform()`
  and `setTransform()`. See [LuaNodeAPI.md](LuaNodeAPI.md#scenetransformscriptnode).
- **Model tools:** `getAnimating` and `setAnimating`, plus any the script declares.
- **Poke:** Has a `pokeScript`, but owns no geometry, so a scene surface never pokes it.

A scripted transform runs its script again on every revisit, so a node used as a point to return to an earlier
frame should be a `SceneTransformNode`, not this type.

### `SceneTextScriptNode`

A single block of text in a scene, anchored in the frame of the transforms visited before it, so it moves,
turns and scales with the geometry around it.

| Field | Required | Meaning |
|---|---|---|
| `coreScript`, `pokeScript` | Yes | The node's two Lua scripts. |
| `text` | No, default `""` | Text to display. A newline breaks it over several lines. |
| `textPosition` | No, default `[0, 0, 0]` | Point the text is centred on, in the frame of the transforms above it. |
| `textHeight` | No, default `1` | Height of one line of the text, in the units of that frame. Must be greater than 0. |
| `textColour` | No, default `[255, 255, 255, 255]` | Red, green, blue and alpha, each 0-255. |
| `textBackground` | No, default no box | Box drawn behind the text: an object with `fillColour` and `borderColour` (each red, green, blue and alpha, 0-255, default `[0, 0, 0, 0]`), `borderWidth` and `padding` (default `0`), and `cornerRadius`, either one radius for every corner or `[topLeft, topRight, bottomRight, bottomLeft]` (default `0`). Lengths are in the same units as `textHeight` and may not be negative. Members left out keep their default. |
| `textVisible` | No, default `true` | Whether the text is shown. Hidden text is left out of the scene. |
| `textChangeMode` | No, default `"DIRECT"` | How a text change action reaching the node is handled. `"DIRECT"` makes the change at once. `"SCRIPT"` hands it to the `coreScript`'s `onTextChange(request)`, which makes whatever change it decides on itself. `"LOCKED"` ignores it. |

- **Actions applied:** Scene, which adds the text to the scene under the current transform. Strobe, Script,
  Animate and Agent, as on `SceneGeometryScriptNode`. Text visible, which shows or hides its text. Text
  change, which changes its text according to `textChangeMode`. It has no agent visible geometry, so takes no
  agent affect action.
- **Emits:** An animate action from `setAnimating()` when asked to, always from the `setAnimating` model
  tool, and from `emitAction()`. A trigger action from `trigger()` or `emitAction()`. A text visible or text
  change action from `emitAction()`.
- **Lua:** The bindings on every scripted node, the scene script node bindings, and its own `setText()`,
  `getText()`, `setTextPosition()`, `getTextPosition()`, `setTextHeight()`, `getTextHeight()`,
  `setTextColour()`, `getTextColour()`, `setTextBackground()`, `getTextBackground()`, `setTextVisible()`,
  `getTextVisible()`, `setTextChangeMode()` and `getTextChangeMode()`, and the `onTextChange(request)` handler
  it may define. See [LuaNodeAPI.md](LuaNodeAPI.md#scenetextscriptnode).
- **Model tools:** `getAnimating` and `setAnimating`, plus any the script declares.
- **Poke:** Has a `pokeScript`, but owns no geometry, so a scene surface never pokes it.

In the WebGL scene view the text always faces the camera and is drawn over the geometry, sized by its distance
the same way geometry is. Once a line of it is too small to read, it is replaced by a marker in the scene: a
signboard with the footprint of the text, thick enough to see edge on, which geometry in front of it can hide.
Right-clicking the text or its marker brings it to the centre of the view, close enough to read.

### `SceneImageNode`

A single image in a scene, anchored in the frame of the transforms visited before it, so it moves, turns and
scales with the geometry around it. It runs no script. On screen it behaves exactly as a `SceneTextScriptNode`'s
text does.

The image comes from exactly one of three places:

| Field | Meaning |
|---|---|
| `imageData` | The image itself, as a base64 PNG kept in the hive definition. Because a hive stored in LMDB keeps each node as this same JSON, an embedded image goes into the database with the rest of the hive. |
| `imageFile` | Path of a PNG file on the machine running the hive, read when the node is built. If the server was started with an image root (`-i`), the path is relative to that directory and must stay within it; otherwise it is relative to the working directory. See [Image files](matrytsya.md#image-files). |
| `imageUrl` | An `http` or `https` address that each viewer loads for itself. |

An embedded or file image must be a PNG, the format libpng reads. It is decoded in full when the node is
built, so a file that is not a PNG, or one that is truncated or corrupt, fails the build instead of reaching a
viewer. Such an image may be no more than 8192 pixels along a side, 16777216 pixels in all, and 16 MiB in size.
A URL image is not limited by format: the server never fetches it, and the viewer's browser draws whatever
the address serves, such as PNG, JPEG, GIF, WebP or SVG.

| Field | Required | Meaning |
|---|---|---|
| `imageData`, `imageFile`, `imageUrl` | Exactly one | Where the image comes from; see above. |
| `imagePosition` | No, default `[0, 0, 0]` | Point the image is centred on, in the frame of the transforms above it. |
| `imageHeight` | No, default `1` | Height of the image, in the units of that frame. Its width follows from its aspect ratio. Must be greater than 0. |
| `imageBackground` | No, default no box | Box drawn behind the image, the same object as `textBackground`. Lengths are in the same units as `imageHeight`. |
| `imageVisible` | No, default `true` | Whether the image is shown. A hidden image is left out of the scene. |

When a hive is saved, a file or URL image is saved as its path or address, not as its bytes, so it is read
again from there on every load. An embedded image is saved as its bytes.

- **Actions applied:** Scene, which adds the image to the scene under the current transform. Image visible,
  which shows or hides it.
- **Emits:** Nothing.
- **Lua, model tools:** None. A `BuilderScriptNode` can create one with `createSceneImageNode{...}`.
- **Poke:** None; it owns no geometry, so a scene surface never pokes it.

In the WebGL scene view the image always faces the camera and is drawn over the geometry, sized by its
distance the same way geometry is. Once it is too small to see, it is replaced by a marker in the scene with
its footprint, filled with its average colour, which geometry in front of it can hide. A URL image's colour is
not known, so its marker takes the box's fill, or grey. Right-clicking the image or its marker brings it to the
centre of the view, filling most of it. An embedded or file image is served to the viewer from the scene
surface's own `image` endpoint and fetched again only when it changes.

### `AgentNode`

Holds a list of prompts and, when triggered, emits them as one agent action. That action carries a single
conversation from node to node, so later prompts see what earlier ones produced.

| Field | Required | Meaning |
|---|---|---|
| `capability` | Yes | `LOW`, `MEDIUM` or `HIGH`. Every prompt is requested for the `NODE` role at this capability, so a harness must assign a model to that exact pair. |
| `prompts` | Yes | At least one entry: `nodeType` (required), `prompt` (required), and optionally `nodeIdentifier` and `terminateOnResponse`. At each node the action reaches, the first entry whose type, and name if given, matches supplies the prompt. A node matching no entry is passed over. |
| `autoTriggerAgentAction` | No, default `true` | Whether a trigger reaching this node emits the agent action. |
| `serialiseEmittedActions` | No, default `true` | Whether only one emitted action may be running at a time, later ones waiting their turn. If false, each trigger starts its own conversation alongside any already running. |

- **Actions applied:** Trigger. It and `BuilderScriptNode` are the only trigger targets.
- **Emits:** An agent action when triggered, unless `autoTriggerAgentAction` is false.
- **Where the agent action has effect:** Only `SceneGeometryScriptNode`, `SceneTransformScriptNode`,
  `TriggerNode` and `BuilderScriptNode` react to an agent action. A prompt written for any other type is never sent. An agent action
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
  poke unless `emitTriggerOnPoke` is false. An animate action, a text visible action and a text change action
  from `emitAction()`.
  None is ever applied back to this node.
- **Lua:** Only the bindings on every scripted node: `NodeType`, `trigger()`, `ToolType`, `ActionType` and
  `emitAction()`. None of the scene script node bindings exist here. See
  [LuaNodeAPI.md](LuaNodeAPI.md#triggernode).
- **Model tools:** `emitTrigger`, plus any the script declares.
- **Poke:** Runs `pokeScript`, then emits a trigger unless `emitTriggerOnPoke` is false. It owns no geometry,
  so a scene surface never pokes it.

### `BuilderScriptNode`

A scripted node whose scripts can grow the hive it is part of while the hive runs. It takes no part in a
scene.

| Field | Required | Meaning |
|---|---|---|
| `coreScript`, `pokeScript` | Yes | The node's two Lua scripts. Either may build. |
| `buildOnce` | No, default `true` | Whether the node builds once only. The first script run (each trigger is one) or script tool call that creates anything is the build, and once it has finished every later attempt is refused, so the same nodes and edges are never built twice. |

- **Actions applied:** Trigger and Script, each of which runs `coreScript`. Being a trigger target is the
  usual way to set a build off: a trigger from another script, a poked `TriggerNode`, a model's `emitTrigger`
  or another hive. Agent, which sends the matching prompt with `emitTrigger` and any script defined tools on
  offer. It supports neither strobe flag, so a strobe never runs its script, and an `init()` runs on the
  first trigger or script action to reach it, not at hive start.
- **Emits:** The same as a `TriggerNode` through `trigger()`, `emitAction()` and the `emitTrigger` model
  tool. A poke never emits anything of its own accord.
- **Lua:** The bindings on every scripted node, plus a create binding for every other type in this document
  (`createPingNode{...}` to `createTriggerNode{...}`), each taking that type's fields, and `createEdge()`,
  which wires any two nodes in the hive. `getHiveName()`, `getSelfName()`, `getNodeNames()`, `nodeExists()`,
  `getNodeInfo()` and `getNodesOfType()` look the hive up, and the `ActionFlag` and `VertexVisibility`
  constants fill the fields that take them. It cannot create another `BuilderScriptNode`. See
  [LuaNodeAPI.md](LuaNodeAPI.md#builderscriptnode).
- **Model tools:** `emitTrigger`, plus any the script declares. A script defined tool that calls the create
  bindings is how a model is given the means to build.
- **Poke:** Runs `pokeScript`. It owns no geometry, so a scene surface never pokes it.

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
| `SceneTextScriptNode` | `SCENE_TEXT_SCRIPT_NODE` |
| `AgentNode` | `AGENT_NODE` |
| `TriggerNode` | `TRIGGER_NODE` |
| `BuilderScriptNode` | `BUILDER_SCRIPT_NODE` |
| `SceneImageNode` | `SCENE_IMAGE_NODE` |

`GRAPH_NODE` and `SCRIPT_NODE` are also listed in that enum, but no concrete type a hive can hold reports
either of them.
