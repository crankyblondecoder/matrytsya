#ifndef HIVE_LOADER_H
#define HIVE_LOADER_H

#include <string>

#include "HiveNodeDescriptor.hpp"
#include "HiveSurfaceDescriptor.hpp"

/**
 * Format-agnostic supplier of the data needed to build a hive: its name, its nodes and the edges
 * between them, its surfaces, and its strobe emitter and strobe surface registrations. HiveBuilder
 * drives construction of a GraphHive entirely through this interface, so a new persisted format
 * only needs a new HiveLoader subclass, never a change to HiveBuilder itself.
 * @note Nodes and surfaces are exposed by index rather than as a stream because HiveBuilder needs
 *       every node/surface to exist before it can wire edges, surfaces, strobe emitters or strobe
 *       surfaces, all of which reference their targets by name that may appear later than the
 *       referencing entry.
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
		 * Get the number of surfaces in the hive being loaded.
		 */
		virtual unsigned getSurfaceCount() = 0;

		/**
		 * Get the descriptor of a single surface.
		 * @param index Index of the surface, in [0, getSurfaceCount()).
		 */
		virtual HiveSurfaceDescriptor getSurface(unsigned index) = 0;

		/**
		 * Get the number of strobe emitter registrations in the hive being loaded.
		 */
		virtual unsigned getStrobeEmitterCount() = 0;

		/**
		 * Get a single strobe emitter registration.
		 * @param index Index of the registration, in [0, getStrobeEmitterCount()).
		 * @param nodeName Set to the name of the node to register as a strobe emitter.
		 * @param periodMs Set to the emission period in milliseconds. Defaults to 33 if not specified.
		 */
		virtual void getStrobeEmitter(unsigned index, std::string& nodeName, unsigned& periodMs) = 0;

		/**
		 * Get the number of strobe surface registrations in the hive being loaded.
		 */
		virtual unsigned getStrobeSurfaceCount() = 0;

		/**
		 * Get a single strobe surface registration.
		 * @param index Index of the registration, in [0, getStrobeSurfaceCount()).
		 * @param surfaceName Set to the name of the surface to register as strobed.
		 * @param periodMs Set to the strobe period in milliseconds. Defaults to 33 if not specified.
		 */
		virtual void getStrobeSurface(unsigned index, std::string& surfaceName, unsigned& periodMs) = 0;
};

#endif
