#ifndef GRAPH_FOCUSABLE_H
#define GRAPH_FOCUSABLE_H

/**
 * Support for a node that can be designated as the visual focus of a view.
 */
class GraphFocusable
{
	public:

		virtual ~GraphFocusable();

		/**
		 * Get whether this node is the initial focus of a view.
		 */
		bool getInitialFocus();

		/**
		 * Set whether this node is the initial focus of a view.
		 */
		void setInitialFocus(bool initialFocus);

		/**
		 * Get the fraction of a view this node should occupy when it is the initial focus.
		 */
		double getFocusViewportFraction();

		/**
		 * Set the fraction of a view this node should occupy when it is the initial focus.
		 */
		void setFocusViewportFraction(double focusViewportFraction);

	protected:

	private:

		/// Whether a viewer initially focuses, in its own way, on this node.
		bool _initialFocus = false;

		/// Fraction (0..1) of a view this node should occupy when it is the initial focus.
		double _focusViewportFraction = 0.5;
};

#endif
