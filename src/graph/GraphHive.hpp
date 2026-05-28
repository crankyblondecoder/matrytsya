#ifndef GRAPH_HIVE_H
#define GRAPH_HIVE_H

/**
 * Types of nodes a have can create and contain.
 */
enum GraphHiveNodeType
{
	/** Simple ping node. */
	PING = 1,
	/** Capable of routing to different edges depending on action. */
	ROUTE = 2,
	/** Can teleport an action between hives. */
	TELEPORT = 3
};

/**
 * A "Hive" is a container for nodes.
 * Nodes can refer to the hive when they want access to specific services, like persistence for example.
 */
class GraphHive
{
    public:

        virtual ~GraphHive();
        GraphHive();

	protected:

    private:
};

#endif
