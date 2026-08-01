# The module covers both the loading and persistence of hive data.

The document doc/LuaNodeAPI.md covers the Lua bindings available to a hive's `coreScript`/`pokeScript` sources, on a per concrete node type basis.

The document doc/hiveSchema.json is a JSON schema that can be used to define a hive which can be loaded into a hive collection running in an instance of Matrytsya.

The document doc/harnessSchema.json is a JSON schema that can be used to define the agentic harness a hive is run against: the providers models are drawn from, and the models, system prompts and tool sets assigned to each role and capability.

## Loading

There are two independent load paths, each built the same way: a format-agnostic `*Loader` interface supplies descriptors, and a `*Builder` turns those descriptors into live objects. Adding a new persisted format means writing a new loader subclass, never touching a builder.

- `HiveLoader` / `HiveBuilder` build a `GraphHive`, with `JsonHiveLoader` reading hiveSchema.json documents.
- `HarnessLoader` / `HarnessBuilder` build an `AgenticHarness`, with `JsonHarnessLoader` reading harnessSchema.json documents.

The two are kept apart because a harness is not part of a hive's structure: the same harness serves whichever hive it is given to, which models a hive runs against can be changed without touching its definition, and a hive can be built and run with no harness at all. `matrytsya_test.cpp` shows the pairing — a hive file and a harness file loaded separately, with a failure to build the harness leaving the hive running with no model behind it.

A loader only validates the shape of what it reads. Every rule that spans more than one entry — a name that must exist elsewhere in the file, a value that must be one the rest of the system recognises — belongs to the builder, so that it is enforced identically whatever format the data arrived in.

Building a harness contacts every provider its definition names, both to confirm the server can be reached and to check that it serves the models assigned to it, so the call blocks for as long as those servers take to answer and fails if one of them cannot be used.

Tool sets are named in a harness definition (e.g. `CHAT`) rather than described, because bindings are built against live objects. `HarnessBuilder` maps each name onto the retrieval method of the hive's `GraphToolBindingsFactory` that supplies it, so a new set of bindings needs a name adding there and to harnessSchema.json. Bindings belonging to a single node are not nameable this way at all: a node level request is given the tools of the node being visited as the action reaches it.
