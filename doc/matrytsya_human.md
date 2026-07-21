# Matrytsya, Explained

Matrytsya is a system for building things out of a network of small, connected pieces — a **hive** — and letting that network come alive by having little bursts of activity travel through it. Those bursts can, for example, build a 3D scene, animate it over time, and let people look at it or interact with it through a browser. How a hive is interpreted and expressed to the outside world is not limited to a 3D scene though.

## The hive: nodes and edges

A hive is made of **nodes** joined together by **edges**. A node is a single small unit of behaviour or content — for example, a piece of 3D geometry, a transform (a move/rotate/resize instruction), or a point that starts things off. An edge is a one-way connection from one node to another, describing where activity is allowed to travel next.

Put enough nodes and edges together and you get a graph that can branch, loop back on itself, and reuse the same nodes for different purposes. A hive can contain several such graphs at once.

## Actions: how things happen

Nothing in a hive does anything on its own — it needs an **action** to visit it. An action is a single thread of activity that starts at one node, does whatever that node defines, then follows exactly one edge to the next node, does its thing there, and so on. It never splits into multiple simultaneous paths — think of it as one messenger with one route, not a flood spreading through the network.

As default, an action keeps track of every edge it has already crossed and won't cross the same one twice, which stops it from getting stuck going around a loop forever. It naturally comes to an end once it reaches a node where every outgoing edge has already been used. New nodes in the future may provide the ability to re-visit edges already traversed but they will be specialised cases.

For the 3D scene case, as an action travels through nodes that describe geometry and transforms, it accumulates those transforms (much like tracking your position and orientation as you follow a set of turn-by-turn directions), and uses the result to place geometry correctly in a scene. This is how a hive builds up a 3D scene: not by storing one static model, but by an action walking the graph and assembling the scene piece by piece as it goes.

## Surfaces: where the hive becomes visible

A hive's nodes and edges are internal — on their own they're not something you could look at or click on. A **surface** is what makes a piece of the hive visible and interactive from the outside. You attach a surface to a starting point in the graph, and when an action runs from there, the surface collects the results — For example, with a scene surface, the geometry, the transforms, the pieces that make up a scene — into something coherent that can be displayed.

A surface also accepts interaction going the other way: if a viewer clicks or otherwise interacts with something the surface is showing, that interaction can be routed back to the specific node responsible for the part that was interacted with — see Poking, below.

You can think of a surface as a window into one particular part of the hive — everything visible through that window is built by actions running inside the hive, but the window itself is what a person on the outside actually sees and touches.

## Maps: publishing a surface to the outside world

A surface by itself only exists inside the running system — something still needs to deliver it to an actual viewer. That job belongs to a **map**. A map takes a surface and publishes it through some concrete external channel, in a format that channel understands.

For example, a map might serve a surface as a web page: it renders an HTML page, exposes the surface's current contents so a browser can fetch them, and keeps the browser up to date as the surface changes — all without the person viewing it needing to reload the page. Different maps can expose the same kind of surface through different technologies; the surface itself doesn't need to know or care how it's being shown.

So the relationship is layered: nodes and edges form the hive, actions travel the hive and do work, a surface gathers the results of that work into something viewable, and a map delivers that surface to the outside world through a particular technology.

## Strobing: keeping things fresh

Some things in a hive are meant to happen repeatedly rather than just once. A node can be registered to fire off a fresh action automatically at a regular frequency — this is called **strobing**. A surface can likewise be strobed, meaning it periodically re-collects the latest results from the hive so that anything watching it (through a map) sees an up-to-date, possibly animated, picture rather than a single frozen snapshot.

This is what allows a scene to appear to move or evolve over time: a strobing node repeatedly sends fresh actions through the graph, a strobing surface repeatedly gathers what those actions produce, and a map keeps delivering the latest version out to whoever is watching.

## Notifications: nodes keeping each other informed

Alongside actions travelling through the graph, a node can quietly keep another node informed about itself. A node can be set up to **listen** to one or more other nodes, so that whenever one of those nodes has something to announce, each of its listeners is told directly.

This is deliberately separate from actions and edges. A notification doesn't travel along edges, doesn't follow a route through the graph, and doesn't carry any of the work an action does — it's simply one node signalling straight to the nodes that have registered an interest in it. The wiring is described from the listener's side: a node names the other nodes it wants to hear from.

Each announcement carries a type describing what kind of thing is being reported, and the mechanism is general — a range of announcement types is expected, with more added over time. It gives nodes a lightweight way to stay coordinated: one node can react to something happening at another without an action needing to visit it.

## Poking: interacting back

Not everything flows outward from the hive to the viewer — a **poke** is how a viewer reaches back in and nudges one specific piece of it. When a surface shows something interactive and a viewer clicks it, presses and holds it, or drags it, that interaction is captured as a poke and delivered through the surface to the exact node responsible for the part that was touched, regardless of which map the viewer was actually using.

A poke carries a type describing the kind of interaction it represents: a brief hit (a click, or a press and release), a grab (a press and hold), or a drag (a click-and-move, which also carries the direction and distance moved). Only nodes that have deliberately opted in to being poked react to one — everything else in the hive simply isn't reachable this way. Poking is not just limited to hit, grab or drag though, it is the mechanism that covers all user input communication with the hive.

A pokeable node keeps its two roles separate: whatever it does as part of ordinary traversal when an action passes through it, and, independently, whatever it does specifically when poked. Poking a node doesn't trigger its ordinary behaviour, and its ordinary behaviour doesn't fire just because it was poked. That separation lets a piece of a scene take part in normal traversal and still react to being clicked or dragged without the two interfering with each other.

## Putting it together

A simple hive might describe a rose: a starting node kicks things off, transform nodes position and orient the petals and stem, and geometry nodes describe their actual shapes. A surface is attached to the starting node to collect everything those nodes produce into one complete scene. A map then publishes that surface as a web page, so opening the page in a browser shows the rose. If both the starting node and the surface are set to strobe, the rose can be redrawn many times a second, allowing it to animate — for instance, gently swaying — with the browser view updating automatically as it does. If one of the petal nodes is made pokeable, clicking that petal in the browser sends a poke back through the surface to that exact node, letting it react on its own — for instance, changing colour or falling away — independently of the swaying animation still being driven by strobing.
