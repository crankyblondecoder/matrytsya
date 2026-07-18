# Examples

Example hive definitions in JSON, matching `src/persist/json/hiveSchema.json`. These are reference
material, not test fixtures — they can be loaded via `JsonHiveLoader` and `HiveBuilder` to build a
working hive, but nothing in the build or test suite depends on them.

- `flowerHive.json` — the seven-petal animated flower hive built procedurally by
  `src/matrytsya_test.cpp`, expressed as a static JSON hive definition instead.
