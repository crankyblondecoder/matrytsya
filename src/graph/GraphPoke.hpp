#ifndef GRAPH_POKE_H
#define GRAPH_POKE_H

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

		/// Type of data that is passed to poke.
		union PokeData
		{
			struct
			{
				/// Hit duration in milliseconds.
				int hitDuration;
			};

			// Nothing for GRAB at this stage.

			struct
			{
				/// Drag vector in world coordinates.
				float dragVector[3];
			};
		};

		/**
		 * @param type Type of poke.
		 * @param magnitudes The poke magnitudes. The meaning of which is type specific.
		 */
		GraphPoke(PokeType type, PokeData data);

		virtual ~GraphPoke();

		/**
		 * Get the type of this poke.
		 */
		PokeType getType();

		/**
		 * For the HIT poke, get the duration of the hit.
		 */
		int getHitDuration();

		/**
		 * Get the drag vector associated with the DRAG poke type.
		 * @param vectorToPopulate Populate this array with the drag vector.
		 */
		void getDragVector(float vectorToPopulate[3]);

	protected:

	private:

		/// Type of this poke.
		PokeType _type;

		/// Data related to the poke. How this is interpreted is specific to the poke type.
		PokeData _data;
};

#endif
