This module covers agentic behaviour for a hive and its graph — the logic that lets nodes act and react on their own, rather than only in direct response to a poke from outside the hive.

An `AgenticHarness` collects the models, system prompts and tool bindings a hive works through, each assigned to a role and the capability that role is served at. It can be assembled in code, or loaded from a definition file by the persist module's `HarnessBuilder` — see doc/harnessSchema.json for the JSON structure and examples/ollamaHarness.json for a complete one.
