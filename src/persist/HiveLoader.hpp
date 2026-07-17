#ifndef HIVE_LOADER_H
#define HIVE_LOADER_H

#include <string>

#include "HiveNodeDescriptor.hpp"

/**
 * Format-agnostic supplier of the data needed to build a hive: its name, its nodes and the edges
 * between them, and its strobe emitter registrations. HiveBuilder drives construction of a
 * GraphHive entirely through this interface, so a new persisted format only needs a new HiveLoader
 * subclass, never a change to HiveBuilder itself.
 * @note Nodes are exposed by index rather than as a stream because HiveBuilder needs every node to
 *       exist before it can wire edges or strobe emitters, which reference nodes by name that may
 *       appear later than the referencing entry.
 */
class HiveLoader
{
	public:

		virtual ~HiveLoader() {}

		/**
		 * Get the canonical name of the hive being loaded.
		 */
		virtual std::string getHiveName() = 0;

		/**
		 * Get the number of nodes in the hive being loaded.
		 */
		virtual unsigned getNodeCount() = 0;

		/**
		 * Get the descriptor of a single node.
		 * @param index Index of the node, in [0, getNodeCount()).
		 */
		virtual HiveNodeDescriptor getNode(unsigned index) = 0;

		/**
		 * Get the number of strobe emitter registrations in the hive being loaded.
		 */
		virtual unsigned getStrobeEmitterCount() = 0;

		/**
		 * Get a single strobe emitter registration.
		 * @param index Index of the registration, in [0, getStrobeEmitterCount()).
		 * @param nodeName Set to the name of the node to register as a strobe emitter.
		 * @param frequencyHz Set to the emission frequency in Hz.
		 */
		virtual void getStrobeEmitter(unsigned index, std::string& nodeName, unsigned& frequencyHz) = 0;
};

#endif
