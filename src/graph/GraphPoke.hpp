#ifndef GRAPH_POKE_H
#define GRAPH_POKE_H

#include <array>

/**
 * Defines a graph "poke".
 * A poke is an interaction with the graph from outside the hive.
 */
class GraphPoke
{
	public:

		/// Type of poke this is.
		enum class PokeType
		{
			/// Something has been hit. Like a button being pressed and released or a mouse click.
			HIT,

			/// Something has been grabbed. Like a button that has been held down, or a mouse click and hold.
			GRAB,

			/// Something has been dragged. Like click and drag across a screen.
			DRAG
		};

		/**
		 * @param type Type of poke.
		 * @param magnitudes The poke magnitudes. The meaning of which is type specific.
		 */
		GraphPoke(PokeType type, std::array<int, 4> magnitudes);

		virtual ~GraphPoke();

		/**
		 * Get the type of this poke.
		 */
		PokeType getType();

		/**
		 * Get the magnitudes associated with this poke.
		 * The meaning of magnitudes is poke type specific.
		 */
		std::array<int, 4> getMagnitudes();

	protected:

	private:

		/// Type of this poke.
		PokeType _type;

		/// Magnitudes of the poke. The meaning of this is poke type specific.
		std::array<int, 4> _magnitudes;
};

#endif
