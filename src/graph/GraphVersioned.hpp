#ifndef GRAPH_VERSIONED_H
#define GRAPH_VERSIONED_H

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

		/// Current version.
		unsigned _version = 1;
};

#endif

