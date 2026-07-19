# Matrytsya Documents Root

> **AI context document.** This file is written to be supplied as context to an AI assistant, not as
> general human-facing documentation. Prefer precision and density over exposition.

## Related documents

- [hiveSchema.json](hiveSchema.json) — JSON schema a hive definition must satisfy: nodes, edges, strobe
  emitters, surfaces, strobe surface registrations.
- [LuaNodeAPI.md](LuaNodeAPI.md) — Lua bindings available to a hive's `coreScript`/`pokeScript` sources,
  per concrete node type.

## Action

An action traverses the hive's graph and is applied to each node it visits, invoking node-specific
behaviour (e.g. ping, animation step).

Traversal model:
- Bound to a starting node; optionally applied there, depending on configuration.
- Applied to a node, then moves along one outgoing edge to the next node, is applied there, repeat.
- Default: an action never traverses the same edge twice. It remembers every edge it has crossed and
  excludes those from selection. This bounds traversal on cyclic graphs.
- Completes (and is no longer applied to any node) once bound to a node with no remaining edge it is
  permitted to traverse.

## Transform behaviour during scene generation

A scene-generation action walks the graph and, at each transform node it visits, contributes that
node's local transform (rotation/translation/scale) into a shared cumulative transform stack for the
scene surface being populated. A new transform is pre-multiplied against the current top of that stack
(standard model-matrix stacking), producing the transform's effective parent-to-node result.

The edge-repetition rule only stops the same *edge* being crossed twice; a graph with multiple paths can
still let a scene-generation action reach the same transform node twice via two different edges. When
that happens, the transform is not recomputed or re-multiplied. The node is looked up by id in the
existing stack, and the already-computed cumulative transform from the first visit is reused (pushed
again as-is). A revisit is therefore a no-op with respect to the transform's contribution — it neither
duplicates nor compounds the transform.
