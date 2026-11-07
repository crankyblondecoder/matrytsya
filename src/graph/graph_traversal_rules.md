# Graph Rules

- Actions are only created by nodes.

- Actions always point to a "current" node.

- Which edge an action traverses is decided by the actions current node.

- Of the edges an action is permitted to cross, the most specifically matched one is taken, whatever order the
  edges were added in. From most to least specific: an edge matching both a key and an action flag, an edge
  matching a key only, an edge matching an action flag only, and an unrestricted edge. Array order only decides
  between edges of equal specificity, where the earliest wins. An unrestricted edge is therefore only crossed
  when no keyed or flagged edge is available to that action.

- An edge with a non-zero traversal key can only be crossed by an action holding that exact key. The key is
  checked in addition to action flags, and is fixed when the edge is created. A key of zero means unkeyed.
  An action is given its keys when it is constructed and cannot gain or lose them afterwards.

- By default an action never crosses the same edge twice. Each action records the edges it has crossed and
  excludes them from later selection, which bounds traversal on a cyclic graph. The record belongs to the
  action, not to the node, so an edge stays available to every other action crossing it.

- Only the crossed edge is excluded, not its siblings. An edge passed over on one visit stays eligible if the
  action reaches that node again by another path.

- An actions energy cannot be increased after it is created. Only decreased.

- Actions are responsible for their own scheduling of thread time requests.

- Actions are applied to a node as soon as they reach it. A node can therefore have several actions applied to
  it simultaneously, and is responsible for synchronising its own state.

