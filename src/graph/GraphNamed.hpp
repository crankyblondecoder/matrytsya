#ifndef GRAPH_NAMED_H
#define GRAPH_NAMED_H

#include <string>

#define GRAPH_HIVE_NAME_MAX_LEN 128

/**
 * Support for uniquely naming a graph object.
 */
class GraphNamed
{
	public:

		virtual ~GraphNamed();

		/**
		 * Get the canonical name of this hive.
		 */
		std::string getName();

		/**
		 * Set the canonical name of this hive.
		 */
		void setName(std::string name);

	protected:

	private:

		/// The name of the hive. Maximum name size appies.
		std::string _name;
};

#endif
