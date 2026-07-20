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
			DRAG,

			/// Something entered into the hovered over state. Like a mouse pointer going over a piece of geometry.
			HOVER_ENTER,

			/// Something left being in the hovered over state. Like a mouse pointer leaving being over a piece of geometry.
			HOVER_LEAVE
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
		 * @param data The poke data. The meaning of which is type specific.
		 * @param chunkId Id of the scene chunk this poke was aimed at, or 0 if the poke is not associated
		 *        with a scene chunk. A chunk id is only unique within its owning node, so this does not
		 *        identify a chunk on its own - it must be paired with the poked node's id.
		 */
		GraphPoke(PokeType type, PokeData data, unsigned chunkId = 0);

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

		/**
		 * Get the id of the scene chunk this poke was aimed at.
		 * @returns The chunk id, or 0 if this poke is not associated with a scene chunk.
		 * @note A chunk id is only unique within its owning node, so it must be paired with the poked
		 *       node's id to identify a specific chunk.
		 */
		unsigned getChunkId();

	protected:

	private:

		/// Type of this poke.
		PokeType _type;

		/// Data related to the poke. How this is interpreted is specific to the poke type.
		PokeData _data;

		/// Id of the scene chunk this poke was aimed at, or 0 if not associated with a chunk. Only unique within the owning node.
		unsigned _chunkId;
};

#endif
