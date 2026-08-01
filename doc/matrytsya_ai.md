# Matrytsya Hive: Model & Authoring Reference

> **AI context document — entry point for generating and manipulating a hive.** Read this first, then the linked schema and Lua API. Precision and density over exposition.

## Related documents

- [hiveSchema.json](hiveSchema.json) — JSON schema a hive definition must satisfy: nodes, edges, strobe emitters, surfaces, strobe surface registrations.
- [harnessSchema.json](harnessSchema.json) — JSON schema an agentic harness definition must satisfy: providers, and the models, system prompts and tool sets assigned to each role and capability. Separate from the hive definition: a harness is not part of a hive's structure, and a hive can run without one.
- [LuaNodeAPI.md](LuaNodeAPI.md) — Lua bindings for a hive's `coreScript`/`pokeScript` sources, per concrete node type.
- [modelTools.md](modelTools.md) — tools a model is given when a hive makes a request of it, and the role and capability each request is made for.

## Action

An action traverses the hive graph, applied to each node it visits, invoking node-specific behaviour (e.g. ping, animation step).

Traversal:
- Bound to one node at a time; never forks or branches. Bound to a starting node, optionally applied there per config.
- Step = apply at current node, cross one outgoing edge, apply at next, repeat.
- Edge selection at a node: exactly one edge, from those permitted by both edge and action. Other edges are skipped, not spawned/queued/deferred.
- Selection order: flag-carrying edges outrank unrestricted ones. The first permitted edge in the edge array (insertion order) that declares `actionFlags` wins; only when no flagged edge is permitted is the first permitted unrestricted edge taken. Array order therefore only decides between edges of equal rank.
- Consequence: an unrestricted edge acts as a fallback route rather than one competing on position. A node can carry a flagged route for a specific action type alongside an unrestricted route for everything else, and the flagged route is taken by the actions it names regardless of which edge was added first.
- Exclusion (default): an action never crosses the same *edge* twice; it records every crossed edge and excludes it from future selection. This bounds traversal on cyclic graphs.
- Only the crossed edge is excluded, not its siblings — a skipped sibling stays eligible if the action later revisits the node by another path.
- Consequence: each action's path is linear step-to-step, but over its lifetime a branch node can contribute several of its edges across separate visits.
- Completes (no longer applied anywhere) once bound to a node with no remaining permitted edge, or once its energy runs out (see [Limits](#limits)).

## Action types

Every action carries one or more action flags, and those flags decide two separate things: which edges it may
cross, and whether it is applied at all at the node it arrives on.

- Edge: an edge with `actionFlags` is crossed only by an action carrying one of them. An edge with none is
  crossed by anything.
- Node: each node type supports a fixed set of flags. An action with *required* flags is applied only at a
  node supporting all of them; an action with only optional flags is applied at a node supporting any one.
- A node an action cannot be applied at is still passed through: the action crosses one of its edges as
  normal, and still spends energy there. Support decides whether anything happens, not whether the action
  arrives.

| Action | Flags carried | Emitted by | Effect where applied |
|---|---|---|---|
| Scene | `SCENE_GRAPH_ACTION` (required) | A surface being strobed, or built for the first time | Collects geometry and transforms into the surface. Runs no script. |
| Strobe | `SCENE_STROBE_GRAPH_ACTION` (required), `SCRIPT_GRAPH_ACTION` | A node registered in `strobeEmitters` | Marks the node as strobing, then runs its `coreScript`. |
| Script | `SCRIPT_GRAPH_ACTION` | Internal | Runs the node's `coreScript`, without marking it as strobing. |
| Animate | `ANIMATE_GRAPH_ACTION` (required) | `setAnimating()` in Lua with emission asked for, and always by the `setAnimating` model tool | Sets the node's animating mode to the one the action carries. Runs no script. |
| Agent | `AGENT_GRAPH_ACTION` (required) | An `agentNode` that a trigger reached, unless `autoTriggerAgentAction` is false | Sends the prompt matching this node, carrying one conversation from node to node. |
| Trigger | `TRIGGER_GRAPH_ACTION`, `SERIALISABLE_GRAPH_ACTION` | `trigger()` in Lua, the `emitTrigger` model tool, or poking a `triggerNode` unless `emitTriggerOnPoke` is false | Fires the node if it is a trigger target; teleports itself if the node is a `teleportNode`. Both obey the trigger's name and type restrictions. |
| Ping | `PING_GRAPH_ACTION`, `SERIALISABLE_GRAPH_ACTION` | Internal | Pings the node. |

Flags each node type supports:

| Node type | Supports |
|---|---|
| `pingNode` | `PING_GRAPH_ACTION` |
| `teleportNode` | `SERIALISABLE_GRAPH_ACTION` |
| `sceneRootNode` | None — it emits rather than receives |
| `sceneGeometryNode`, `sceneTransformNode` | `SCENE_GRAPH_ACTION`, `SCENE_STROBE_GRAPH_ACTION` |
| `sceneGeometryScriptNode`, `sceneTransformScriptNode` | `SCENE_GRAPH_ACTION`, `SCENE_STROBE_GRAPH_ACTION`, `SCRIPT_GRAPH_ACTION`, `ANIMATE_GRAPH_ACTION`, `AGENT_GRAPH_ACTION` |
| `triggerNode` | `SCRIPT_GRAPH_ACTION`, `AGENT_GRAPH_ACTION` |
| `agentNode` | `TRIGGER_GRAPH_ACTION` |

Consequences worth authoring to:

- A `triggerNode` supports neither strobe flag, so a strobe never runs its `coreScript`. Its script runs only
  under a script action.
- Only a `teleportNode` supports `SERIALISABLE_GRAPH_ACTION`, so a trigger action leaves the hive only by
  reaching one. `SERIALISABLE_GRAPH_ACTION` on an edge admits trigger and ping actions, being the flag both
  carry.
- Flagging an edge with the two scene flags reserves it for scene building and scene strobing, keeping animate,
  agent and trigger actions out of it. This is the usual way to give a scene pathway a route that the traffic
  driving behaviour does not follow.

## Strobing

Two registrations, independent of each other, and an animated scene needs both:

- `strobeEmitters` registers a node to emit a strobe action into the graph every `periodMs`. Only a
  `sceneRootNode` can be registered. This is what advances state: it runs each scripted node's `coreScript`
  as it reaches it, marking that node as strobing for the duration of the run, which is what `getStrobe()`
  reports.
- `strobeSurfaces` registers a surface to re-run a scene action from its `sceneRootNodeName` every `periodMs`.
  This is what publishes state: it collects the geometry and transforms into the surface for a viewer to
  fetch.

With only the emitter registered, scripts run and nothing published ever changes. With only the surface
registered, no `coreScript` ever runs: script-built geometry is never built at all, and the surface publishes
only what was set directly in JSON. The two periods need not match, and neither has to divide the other.

A scene action rebuilds the surface only when something it reaches reports a changed version, so a scene that
is not moving costs nothing to keep strobing.

## Limits

- **Energy.** An action is created with 512 units and spends one at each node it is bound to, whether or not
  it is applied there, so it dies after at most 512 node visits however the graph is shaped. A scene action
  built for a surface is the exception, given a much larger budget (16535 at present) because it must reach
  the whole of a scene pathway. Revisits count: a graph whose traversal costs more than this is truncated
  part way, silently and always at the same place.
- **Edges per node.** At most 32 outgoing edges on any one node.
- **Hive name.** At most 128 characters.

## Transform behaviour during scene generation

A scene-generation action walks the graph and, at each transform node, contributes that node's local transform (rotation/translation/scale) into a shared cumulative transform stack for the scene surface being populated.

- New cumulative = current top of the stack × the node's own transform (standard model-matrix stacking: parent × local) → the transform's effective parent-to-node result. A node's transform is therefore authored in the frame established by the transform nodes visited before it, never in world space.
- Geometry is placed by whatever is on top of the stack when the geometry node is visited.
- The stack is push-only within a pass: continuing the traversal never undoes a contribution.
- The edge-not-node exclusion means multiple paths can still reach the same transform node twice via different edges.
- On revisit: the node is looked up by id in the existing stack and its already-computed cumulative transform (from the first visit) is re-pushed as-is. No recompute, no re-multiply.
- A revisit is therefore a no-op for the transform's contribution — neither duplicated nor compounded — and is the only way back to an earlier frame. Authoring idiom: end each subtree with an edge back to the transform node that defines the shared frame, so that node's next edge continues into the following subtree from that frame.
- A transform node cannot be shared between parent frames: a revisit replays its first cumulative result rather than recomposing, so each frame a transform is needed in requires its own node.
- A revisit is a visit for every other purpose: a script node's core script runs again, so a node used as a frame-reset point should be a plain transform node, not a scripted one.
