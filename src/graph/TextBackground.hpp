#ifndef TEXT_BACKGROUND_H
#define TEXT_BACKGROUND_H

#include <cstddef>

#include <math.h>

/**
 * A box drawn behind a block of scene text. Lengths are in the units of the frame the text is collected under,
 * the same units as the text's line height, so the box scales along with the text.
 * @note The defaults are fully transparent with no border, padding or rounding, i.e. no visible box at all.
 */
struct TextBackground
{
	/// Corners in the order cornerRadii holds them.
	enum Corner
	{
		TOP_LEFT,
		TOP_RIGHT,
		BOTTOM_RIGHT,
		BOTTOM_LEFT,
		/// Number of corners; not itself a corner.
		CORNER_COUNT
	};

	/// Fill colour: R, G, B, A.
	std::byte fillColour[4] = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};

	/// Border colour: R, G, B, A.
	std::byte borderColour[4] = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};

	/// Width of the border. Zero for no border.
	double borderWidth = 0.0;

	/// Space between the text and the inside of the border, on every side.
	double padding = 0.0;

	/// Radius of each corner, indexed by Corner. Zero for a square corner.
	double cornerRadii[CORNER_COUNT] = {0.0, 0.0, 0.0, 0.0};

	/**
	 * @returns Whether every length is finite and not negative.
	 */
	bool isValid() const
	{
		if(!__isValidLength(borderWidth) || !__isValidLength(padding)) return false;

		for(int corner = 0; corner < CORNER_COUNT; corner++) if(!__isValidLength(cornerRadii[corner])) return false;

		return true;
	}

	bool operator== (const TextBackground& other) const
	{
		if(borderWidth != other.borderWidth || padding != other.padding) return false;

		for(int channel = 0; channel < 4; channel++)
		{
			if(fillColour[channel] != other.fillColour[channel] || borderColour[channel] != other.borderColour[channel])
			{
				return false;
			}
		}

		for(int corner = 0; corner < CORNER_COUNT; corner++) if(cornerRadii[corner] != other.cornerRadii[corner]) return false;

		return true;
	}

	bool operator!= (const TextBackground& other) const
	{
		return !(*this == other);
	}

	private:

		// Written so that a NaN fails it too.
		static bool __isValidLength(double length)
		{
			return length >= 0.0 && isfinite(length);
		}
};

#endif
