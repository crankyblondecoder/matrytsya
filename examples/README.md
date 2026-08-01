# Examples

Example hive and agentic harness definitions in JSON. These are reference materials, not test fixtures.

`flowerHive.json`, `roseHive.json` and `engineHive.json` are hive definitions. `engineHive.json` is a single
piston engine -- crankshaft, connecting rod and piston -- animated as one assembly: each of the three
transforms works its own pose out from the same crank angle, and a 3D button to the left of the engine, level
with the crankshaft, toggles the animate mode across the whole of it. `ollamaHarness.json` is a harness definition: the models, system prompts and tool sets a hive is run against. It is loaded separately from the hive and serves whichever hive it is given to. It names one specific Ollama server and one model served by it, so both have to be reachable for a hive run against it to have a model behind it.

## How to create more examples

Reference `doc/hiveSchema.json` and `doc/LuaNodeAPI.md` and create a new JSON file matching the JSON schema.

For a harness, reference `doc/harnessSchema.json` in the same way.
