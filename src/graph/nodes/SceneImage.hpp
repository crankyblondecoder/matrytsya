#ifndef SCENE_IMAGE_H
#define SCENE_IMAGE_H

#include "../GraphVersioned.hpp"
#include "../ImageSource.hpp"
#include "../TextBackground.hpp"
#include "../../util/Handle.hpp"
#include "../../thread/thread.hpp"

class GraphHiveSceneSurface;

/**
 * Shared image store and population API for scene image nodes. Holds a single image along with where it is
 * anchored, how tall it is and the box drawn behind it.
 * @note Actions are applied to a node simultaneously, so an action showing or hiding the image can run at the
 *       same time as a scene action reading it. Every access to the image state is therefore synchronised.
 */
class SceneImage : public GraphVersioned
{
	public:

		virtual ~SceneImage();

		/**
		 * Set the image to display.
		 * @param source Image to display.
		 */
		void setSource(const ImageSource& source);

		/**
		 * @returns The image to display.
		 */
		ImageSource getSource() const;

		/**
		 * Set where the image is anchored, in the frame of the transform it is collected under.
		 * @param position Anchor position: X, Y, Z. The image is centred on this point.
		 */
		void setPosition(const double position[3]);

		/**
		 * Get where the image is anchored.
		 * @param position Set to the anchor position: X, Y, Z.
		 */
		void getPosition(double position[3]) const;

		/**
		 * Set how tall the image is, in the units of the frame it is collected under. Its width follows from its
		 * aspect ratio.
		 * @param height Image height. Must be greater than zero.
		 * @returns False, leaving the height unchanged, if the given height is not greater than zero.
		 */
		bool setHeight(double height);

		/**
		 * @returns How tall the image is.
		 */
		double getHeight() const;

		/**
		 * Set the box drawn behind the image.
		 * @param background Background box. Its lengths are in the same units as the image height.
		 * @returns False, leaving the background unchanged, if the given background is not valid.
		 */
		bool setBackground(const TextBackground& background);

		/**
		 * @returns The box drawn behind the image.
		 */
		TextBackground getBackground() const;

		/**
		 * Set whether the image is shown.
		 * @param visible True to show the image, false to hide it.
		 */
		void setVisible(bool visible);

		/**
		 * @returns Whether the image is shown.
		 */
		bool getVisible() const;

		/**
		 * Populate the given scene surface with the image from this.
		 * @note A hidden image adds nothing to the surface.
		 * @param surface Surface to populate.
		 * @param nodeId Node ID to use when populating surface.
		 */
		void populateSurface(Handle<GraphHiveSceneSurface> surface, unsigned nodeId);

	private:

		/// Guards everything below. Mutable so that the const read-only accessors can still take it.
		mutable ThreadMutex _lock;

		/// Image to display.
		ImageSource _source;

		/// Anchor position: X, Y, Z.
		double _position[3] = {0.0, 0.0, 0.0};

		/// Height of the image.
		double _height = 1.0;

		/// Box drawn behind the image. Transparent, so not drawn, by default.
		TextBackground _background;

		/// Whether the image is shown.
		bool _visible = true;
};

#endif
