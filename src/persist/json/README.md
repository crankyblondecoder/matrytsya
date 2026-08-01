# This module covers loading and saving of hive data using JSON.

doc/hiveSchema.json describes the JSON structure that is used to define a hive.

doc/harnessSchema.json describes the JSON structure that is used to define the agentic harness a hive is run against.

Both loaders read documents matching their schema; neither validates arbitrary JSON against it. Unrecognised fields are ignored rather than rejected.

