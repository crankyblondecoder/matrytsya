# Graph Rules

- Actions are only created by nodes.

- Actions always point to a "current" node.

- Which edge an action traverses is decided by the actions current node.

- An actions energy cannot be increased after it is created. Only decreased.

- Actions are responsible for their own scheduling of thread time requests.

- After any initial connection to/from a node, when the node no longer has any edges point to/from it, it is deleted.

