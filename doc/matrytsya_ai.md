# Matrytsya Hive: Model & Authoring Reference

> **AI context document — entry point for generating and manipulating a hive.** Read this first, then the linked schema and Lua API. Precision and density over exposition.

## Related documents

- [hiveSchema.json](hiveSchema.json) — JSON schema a hive definition must satisfy: nodes, edges, strobe emitters, surfaces, strobe surface registrations.
- [LuaNodeAPI.md](LuaNodeAPI.md) — Lua bindings for a hive's `coreScript`/`pokeScript` sources, per concrete node type.

## Action

An action traverses the hive graph, applied to each node it visits, invoking node-specific behaviour (e.g. ping, animation step).

Traversal:
- Bound to one node at a time; never forks or branches. Bound to a starting node, optionally applied there per config.
- Step = apply at current node, cross one outgoing edge, apply at next, repeat.
- Edge selection at a node: exactly one edge — the first in the edge array (insertion order) permitted by both edge and action. Other edges are skipped, not spawned/queued/deferred.
- Exclusion (default): an action never crosses the same *edge* twice; it records every crossed edge and excludes it from future selection. This bounds traversal on cyclic graphs.
- Only the crossed edge is excluded, not its siblings — a skipped sibling stays eligible if the action later revisits the node by another path.
- Consequence: each action's path is linear step-to-step, but over its lifetime a branch node can contribute several of its edges across separate visits.
- Completes (no longer applied anywhere) once bound to a node with no remaining permitted edge.

## Transform behaviour during scene generation

A scene-generation action walks the graph and, at each transform node, contributes that node's local transform (rotation/translation/scale) into a shared cumulative transform stack for the scene surface being populated.

- New transform is pre-multiplied against the current top of the stack (standard model-matrix stacking) → the transform's effective parent-to-node result.
- The edge-not-node exclusion means multiple paths can still reach the same transform node twice via different edges.
- On revisit: the node is looked up by id in the existing stack and its already-computed cumulative transform (from the first visit) is re-pushed as-is. No recompute, no re-multiply.
- A revisit is therefore a no-op for the transform's contribution — neither duplicated nor compounded.
