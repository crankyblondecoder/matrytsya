#ifndef GRAPH_NODE_LOCATION_H
#define GRAPH_NODE_LOCATION_H

#include <string>

/**
 * Describes the location of a GraphNode in terms of its host machine, hive, and node name.
 */
class GraphNodeLocation
{
	public:

		GraphNodeLocation();

		GraphNodeLocation(const GraphNodeLocation& copyFrom);
		GraphNodeLocation& operator= (const GraphNodeLocation& copyFrom);

		~GraphNodeLocation();

		/**
		 * Get the hostname of the host machine.
		 * @returns Hostname string.
		 */
		std::string getHostname();

		/**
		 * Test whether the hostname is localhost.
		 * @returns True if the hostname is localhost.
		 */
		bool isLocalHost();

		/**
		 * Set the hostname of the host machine.
		 * @param hostname Hostname string.
		 */
		void setHostname(std::string hostname);

		/**
		 * Get the port of the host machine.
		 * @returns Port number.
		 */
		int getPort();

		/**
		 * Set the port of the host machine.
		 * @param port Port number.
		 */
		void setPort(int port);

		/**
		 * Get the name of the hive containing the node.
		 * @returns Hive name.
		 */
		std::string getHiveName();

		/**
		 * Set the name of the hive containing the node.
		 * @param hiveName Hive name.
		 */
		void setHiveName(std::string hiveName);

		/**
		 * Get the name of the node.
		 * @returns Node name.
		 */
		std::string getNodeName();

		/**
		 * Set the name of the node.
		 * @param nodeName Node name.
		 */
		void setNodeName(std::string nodeName);

	protected:

	private:

		/// Hostname of the host machine.
		std::string _hostname;

		/// Port of the host machine.
		int _port = 0;

		/// Name of the hive containing the node.
		std::string _hiveName;

		/// Name of the node.
		std::string _nodeName;
};

#endif
