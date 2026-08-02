# Model Tools

This document lists the tools a model is given when a hive makes a request of it: the node level tools that
come from the node being visited, and the hive level sets a harness assigns through `toolBindings`. It also
gives the role and capability each kind of request is made for, which is what a harness's assignments must
name to answer it.

## Which role a request is made for

A harness assignment answers a request only if it names that request's exact role and capability pair, so
authoring a harness for a hive means knowing which pair the hive will ask for.

| Request | Role | Capability |
|---|---|---|
| Prompts an `agentNode` emits as an agent action | `NODE` | The `capability` set on that `agentNode`. |
| A message typed at a surface's chat interface | `CHAT` | Chosen by whatever hosts the chat interface, not by the hive. |

`HIVE` and `SCRIPT` are defined roles, but nothing requests them yet. Assignments naming them load without
complaint and go unused.

A hive containing one `agentNode` with `"capability": "LOW"` therefore needs a `NODE`/`LOW` model assignment,
and nothing else, to run. A pair with no model assignment fails the request when it is made; a pair with no
system prompt entry is served with no system prompt at all.

## Node level tools

While an agent action is being applied to a node, the model servicing that node's prompt is given the tools
of that node, on top of whatever set the harness assigned to `NODE` at that capability. The tools belong to
that one node: they are withdrawn as soon as the action moves on, so a prompt written for a later node cannot
reach back to an earlier one.

| Node type | Tools offered |
|---|---|
| `sceneGeometryScriptNode` | `getAnimating`, `setAnimating` |
| `sceneTransformScriptNode` | `getAnimating`, `setAnimating` |
| `triggerNode` | `emitTrigger` |
| `pingNode`, `teleportNode`, `sceneRootNode`, `sceneGeometryNode`, `sceneTransformNode`, `agentNode` | None |

A prompt may still match a node that offers no tools; the model is then simply asked to answer, with the
conversation carried forward as usual.

### Script defined tools

The three node types above are the ones an agent action can reach, and each of them may offer more than the
fixed set listed for it. A `coreScript` that defines a `getToolCallBindings()` global declares tools of its
own, which are added to whatever its node type already offers. These belong to the **node instance** rather
than to the node type: two nodes of the same type running different scripts offer the model different tools,
and a script can vary what it declares by the capability of the model asking.

Defining `getToolCallBindings()` on any other node type does nothing, because no agent action reaches one.

Script defined tools are withdrawn along with the rest when the action moves on, and a script implementing
one badly costs the model nothing: a tool that cannot be called is dropped before the model is told about
it, and one that fails while being called is reported back as a failure the model can correct for. See
[`getToolCallBindings(capability)`](LuaNodeAPI.md#gettoolcallbindingscapability) for how a script declares
them.

### `getAnimating`

Takes no arguments. Returns `animating`, a boolean: whether the node is currently in animating mode.

### `setAnimating`

Takes `animating`, a boolean. Returns `animating`, the mode the node is now in.

**This tool always emits an animate action**, carrying the mode out to every node that action reaches from
here. It is treated as the model's equivalent of poking the node, and it differs from the Lua
`setAnimating(animating, [emitAnimateAction])` binding, whose emission is off unless asked for. A model
setting the mode on one node therefore sets it across the whole subgraph reachable from that node by an
animate action, which is what makes a single node serve as a start/stop control for an assembly.

### `emitTrigger`

Takes two optional arguments, either of which may be left out:

| Argument | Effect |
|---|---|
| `nodeName` | Only nodes with this name are triggered. Left out means any name. |
| `nodeType` | Only nodes of this type are triggered. One of the node type names listed under `nodeType` in `hiveSchema.json`. Left out means any type. |

Returns `triggered`, a boolean: whether the action was emitted.

The emitted action is never applied back to the node that emitted it, so a `triggerNode` cannot trigger
itself. Restrictions apply to everything the trigger action does, not just to trigger targets: a restricted
trigger is also passed over by a `teleportNode` whose name or type does not match, and so is never forwarded
on. A trigger meant to leave the hive through a teleport must therefore either carry no restriction at all or
name that teleport node. See [Triggering other nodes](LuaNodeAPI.md#triggering-other-nodes) for what a trigger
action does where it lands.

## Hive level tool sets

`toolBindings` in a harness assigns one of two named sets to a role and capability pair. Both sets currently
hold the same two tools; they are named separately because what a chat message may reach and what hive level
planning may reach are separate questions.

| Set | Tools |
|---|---|
| `CHAT` | `getNodeNames`, `getNodeId` |
| `HIVE` | `getNodeNames`, `getNodeId` |

### `getNodeNames`

Takes no arguments. Returns `nodeNames`, the list of names of the nodes the hive holds.

### `getNodeId`

Takes `nodeName`, a string, matched exactly. Returns `nodeId`, that node's id.

Because the match is exact, a system prompt for a role holding these tools is worth writing to tell the model
to list names before asking for an id rather than guessing at spelling.
