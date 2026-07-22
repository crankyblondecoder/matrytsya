#ifndef GRAPH_HIVE_SURFACE_H
#define GRAPH_HIVE_SURFACE_H

#include "../util/EventEmitter.hpp"
#include "../util/RefCounted.hpp"
#include "../util/Handle.hpp"
#include "GraphHive.hpp"
#include "GraphHiveSurfaceListener.hpp"
#include "GraphNamed.hpp"
#include "GraphPoke.hpp"

/**
 * Represents a "surface" that a hive can interact with, either for display or input.
 * It essentially provides a layer of abstraction for various display types but keeps the interface that an action
 * has to use consistent.
 */
class GraphHiveSurface : public RefCounted, public GraphNamed, public EventEmitter<GraphHiveSurfaceListener>
{
	public:

		/// Identifies the concrete subclass of a surface, so callers can find a specific kind without an RTTI cast.
		enum class Type
		{
			/// A GraphHiveSceneSurface.
			SCENE_SURFACE
		};

		/**
		 * @param type Concrete type of this surface, as reported by getType().
		 */
		GraphHiveSurface(Type type);

		/**
		 * Get the concrete type of this surface.
		 */
		Type getType();

		/**
		 * Get whether this surface is the default surface of its kind within its hive.
		 */
		bool getDefault();

		/**
		 * Set whether this surface is the default surface of its kind within its hive.
		 * @param isDefault Whether this surface is default.
		 */
		void setDefault(bool isDefault);

		/**
		 * Set the hive this surface is bound to.
		 * @param hive Hive this surface is to be bound to. Must be a valid handle.
		 */
		void setHive(Handle<GraphHive> hive);

		/**
		 * Activate this surface.
		 * This must be done for the surface to start interacting with the hive.
		 */
		virtual void activate() = 0;

		/**
		 * Request this surface to go into population mode (start populate pass).
		 * @param populateVersion The version to assign to this populate pass.
		 * @returns True if could go into population mode, false otherwise. It will return false if already in
		 *          population mode.
		 */
		bool populateStart(unsigned populateVersion);

		/**
		 * Request this surface to go out of population mode.
		 */
		void populateEnd();

		/**
		 * Get whether this surface is in population mode.
		 */
		bool isPopulating();

		/**
		 * Get the version assigned to the last populate pass.
		 */
		unsigned getPopulateVersion();

		/**
		 * Clean up and dereference this surface.
		 */
		virtual void close() final;

		/**
		 * Poke this surface.
		 * @param nodeId The id of the node that is to be poked.
		 * @param poke Poke to apply.
		 */
		virtual void poke(unsigned nodeId, GraphPoke poke);

		/**
		 * Strobe this surface.
		 * This causes it to update/regenerate if required.
		 */
		virtual void strobe() = 0;

	protected:

		// Required by ref counting.
		virtual ~GraphHiveSurface();

		/**
		 * Subclass hook to indicate it should close.
		 */
		virtual void _close() = 0;

		/**
		 * Subclass hook to inform that population has started.
		 */
		virtual void _populateStart() = 0;

		/**
		 * Subclass hook to inform that population has ended.
		 */
		virtual void _populateEnd() = 0;

		/**
		 * Emit the surface changed event.
		 */
		void _emitSurfaceChanged();

	private:

		// Disable copying.
		GraphHiveSurface(const GraphHiveSurface& copyFrom);
		GraphHiveSurface& operator= (const GraphHiveSurface& copyFrom);

		/// Concrete type of this surface.
		Type _type;

		/// Whether this surface is the default surface of its kind within its hive.
		bool _default = false;

		/// Whether this surface is currently in population mode.
		bool _populating = false;

		/// Generic lock.
		ThreadMutex _lock;

		/// Hive this surface is bound to.
		Handle<GraphHive> _hive;

		/// The version assigned to that last populate pass.
		unsigned _populateVersion = 0;
};

#endif
