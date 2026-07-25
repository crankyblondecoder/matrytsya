This module holds concrete ModelToolBindings implementations that let an AI model act on a hive's graph.

It is a separate module from agent because these bindings depend on both agent (ModelToolBindings) and graph (GraphHive, GraphNode), and agent's own module cannot depend on graph without creating a circular link dependency, since graph in turn depends on agent (AgenticHarness, ModelContext).
