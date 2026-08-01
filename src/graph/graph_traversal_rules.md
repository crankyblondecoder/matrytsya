# Graph Rules

- Actions are only created by nodes.

- Actions always point to a "current" node.

- Which edge an action traverses is decided by the actions current node.

- An edge carrying action flags is preferred over an unrestricted edge. Of the edges an action is permitted to
  cross, the first flagged one is taken, whatever order the edges were added in. An unrestricted edge is only
  crossed when no flagged edge is available to that action.

- By default an action never crosses the same edge twice. Each action records the edges it has crossed and
  excludes them from later selection, which bounds traversal on a cyclic graph. The record belongs to the
  action, not to the node, so an edge stays available to every other action crossing it.

- Only the crossed edge is excluded, not its siblings. An edge passed over on one visit stays eligible if the
  action reaches that node again by another path.

- An actions energy cannot be increased after it is created. Only decreased.

- Actions are responsible for their own scheduling of thread time requests.

- Actions are applied to a node as soon as they reach it. A node can therefore have several actions applied to
  it simultaneously, and is responsible for synchronising its own state.

