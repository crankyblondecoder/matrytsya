#ifndef SCENE_TEXT_H
#define SCENE_TEXT_H

#include <cstddef>
#include <string>

#include "../GraphVersioned.hpp"
#include "../TextBackground.hpp"
#include "../../util/Handle.hpp"
#include "../../thread/thread.hpp"

class GraphHiveSceneSurface;

/**
 * Shared text store and population API for scene text nodes. Holds a single block of text along with where it
 * is anchored, how tall a line of it is, what colour it is and the box drawn behind it, so that any node that puts text into a scene
 * reuses a single implementation.
 * @note Actions are applied to a node simultaneously, so a script changing the text can run at the same time as
 *       a scene action reading it. Every access to the text state is therefore synchronised.
 */
class SceneText : public GraphVersioned
{
	public:

		virtual ~SceneText();

		/**
		 * Set the text to display.
		 * @param text Text to display. May contain '\n' to break it over several lines.
		 */
		void setText(const std::string& text);

		/**
		 * @returns The text to display.
		 */
		std::string getText() const;

		/**
		 * Set where the text is anchored, in the frame of the transform it is collected under.
		 * @param position Anchor position: X, Y, Z. The text is centred on this point.
		 */
		void setPosition(const double position[3]);

		/**
		 * Get where the text is anchored.
		 * @param position Set to the anchor position: X, Y, Z.
		 */
		void getPosition(double position[3]) const;

		/**
		 * Set how tall a single line of the text is, in the units of the frame it is collected under.
		 * @param height Line height. Must be greater than zero.
		 * @returns False, leaving the height unchanged, if the given height is not greater than zero.
		 */
		bool setHeight(double height);

		/**
		 * @returns How tall a single line of the text is.
		 */
		double getHeight() const;

		/**
		 * Set the colour of the text.
		 * @param colour Colour: R, G, B, A.
		 */
		void setColour(const std::byte colour[4]);

		/**
		 * Get the colour of the text.
		 * @param colour Set to the colour: R, G, B, A.
		 */
		void getColour(std::byte colour[4]) const;

		/**
		 * Set the box drawn behind the text.
		 * @param background Background box. Its lengths are in the same units as the line height.
		 * @returns False, leaving the background unchanged, if the given background is not valid.
		 */
		bool setBackground(const TextBackground& background);

		/**
		 * @returns The box drawn behind the text.
		 */
		TextBackground getBackground() const;

		/**
		 * Set whether the text is shown.
		 * @param visible True to show the text, false to hide it.
		 */
		void setVisible(bool visible);

		/**
		 * @returns Whether the text is shown.
		 */
		bool getVisible() const;

		/**
		 * Populate the given scene surface with the text from this.
		 * @note Hidden text adds nothing to the surface.
		 * @param surface Surface to populate.
		 * @param nodeId Node ID to use when populating surface.
		 */
		void populateSurface(Handle<GraphHiveSceneSurface> surface, unsigned nodeId);

	private:

		/// Guards everything below. Mutable so that the const read-only accessors can still take it.
		mutable ThreadMutex _lock;

		/// Text to display.
		std::string _text;

		/// Anchor position: X, Y, Z.
		double _position[3] = {0.0, 0.0, 0.0};

		/// Height of a single line of the text.
		double _height = 1.0;

		/// Colour: R, G, B, A.
		std::byte _colour[4] = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};

		/// Box drawn behind the text. Transparent, so not drawn, by default.
		TextBackground _background;

		/// Whether the text is shown.
		bool _visible = true;
};

#endif
