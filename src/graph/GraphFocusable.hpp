#ifndef GRAPH_FOCUSABLE_H
#define GRAPH_FOCUSABLE_H

#include "actionTargets/FocusActionTarget.hpp"
#include "../thread/thread.hpp"
#include "../util/Handle.hpp"

class GraphHiveSceneSurface;

/**
 * Support for a node that can be designated as the visual focus of a view.
 */
class GraphFocusable : public FocusActionTarget
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

		virtual void requestFocus(double viewportFraction = 0) override;

		/**
		 * Get how many times this node has been asked to be brought into focus.
		 * @returns The focus request count, 0 if it never has been. Only ever increases.
		 */
		unsigned getFocusRequestCount();

	protected:

		/**
		 * Pass this node's focus request count, and how the latest request asked to be focused on, on to a
		 * surface being populated, so that it can tell whether a new request has been made since it last looked.
		 * @param surface Surface being populated.
		 * @param nodeId Id of the node this is.
		 */
		void _populateFocusRequest(Handle<GraphHiveSceneSurface> surface, unsigned nodeId);

	private:

		/// How many times this node has been asked to be brought into focus. Guarded by _focusLock, as a focus
		/// action can be applied at the same time as a scene action reads it.
		unsigned _focusRequestCount = 0;

		/// Viewport fraction the latest focus request asked for, 0 for right-click behaviour. Guarded by
		/// _focusLock, so that it is always read together with the request count it belongs to.
		double _requestedViewportFraction = 0;

		/// Guards the focus request state.
		ThreadMutex _focusLock;

		/// Whether a viewer initially focuses, in its own way, on this node.
		bool _initialFocus = false;

		/// Fraction (0..1) of a view this node should occupy when it is the initial focus.
		double _focusViewportFraction = 0.5;
};

#endif
