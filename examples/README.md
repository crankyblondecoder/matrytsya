# Examples

Example hive and agentic harness definitions in JSON. These are reference materials, not test fixtures.

`flowerHive.json`, `roseHive.json` and `engineHive.json` are hive definitions. `engineHive.json` is a single
piston engine -- crankshaft, connecting rod and piston -- animated as one assembly: each of the three
transforms works its own pose out from the same crank angle, and a 3D button to the left of the engine, level
with the crankshaft, toggles the animate mode across the whole of it. The same button can be worked by a
model instead of a pointer: a trigger node feeds an agent node, whose one prompt has the model call a
`toggleEngine` tool that the button's own `coreScript` declares through `getToolCallBindings()`, so a trigger
starts or stops the engine. The tool set varies by the capability of the model asking: a `LOW` model, which is
what the agent node asks for, is given that one no-argument call, while anything more capable is given a
`getEngineRunning`/`setEngineRunning` pair instead. That trigger is what
`flowerHive.json` reaches across to: its agent conversation now ends on a trigger node, where the model calls
an `emitTrigger` tool, and a teleport node forwards the emitted action into the engine hive. The two are
loaded into one collection by the test harness, so poking flower 3 walks the flower conversation and then
starts or stops the engine. `gardenHive.json` is `flowerHive.json` with all of the agentic behaviour taken out: no agent, trigger or teleport nodes, no agent or trigger edges, no tool bindings, and no agent-only geometry, so it runs without a harness or a model. `ollamaHarness.json` is a harness definition: the models, system prompts and tool sets a hive is run against. It is loaded separately from the hive and serves whichever hive it is given to. It names one specific Ollama server and one model served by it, so both have to be reachable for a hive run against it to have a model behind it.

## How to create more examples

Start with `doc/matrytsya_ai.md`, the entry point for generating and manipulating a hive: it explains the model and links the schema and Lua API below.

Reference `doc/hiveSchema.json` and `doc/LuaNodeAPI.md` and create a new JSON file matching the JSON schema.

For a harness, reference `doc/harnessSchema.json` in the same way.

For a hive with agent or trigger nodes in it, reference `doc/modelTools.md` as well: it gives the tools a
prompt can ask a model to call at each node type, and the role and capability pair a harness must assign for
those prompts to be answered at all.
