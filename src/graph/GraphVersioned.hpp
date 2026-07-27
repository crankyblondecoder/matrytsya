#ifndef GRAPH_VERSIONED_H
#define GRAPH_VERSIONED_H

#include <atomic>

class GraphVersioned
{
	public:

		/**
		 * Get the current version.
		 */
		unsigned getVersion() { return _version; };

	protected:

		GraphVersioned(){};
		virtual ~GraphVersioned(){};

		/**
		 * Increment the version.
		 */
		void _bumpVersion() { _version++; };

	private:

		/// Current version. Atomic because actions are applied to a node simultaneously, so this can be
		/// bumped and read from several threads at once.
		std::atomic<unsigned> _version{1};
};

#endif

