#ifndef SCENE_IMAGE_NODE_H
#define SCENE_IMAGE_NODE_H

#include "../actionTargets/ImageVisibleActionTarget.hpp"
#include "../actionTargets/SceneActionTarget.hpp"
#include "../GraphFocusable.hpp"
#include "../GraphNode.hpp"
#include "SceneImage.hpp"

/**
 * Graph node that puts an image into a scene, anchored in the frame of the transforms it is collected under. It
 * runs no script: its image is set through its C++ API, it can be shown or hidden by an image visible action
 * reaching it, and a focus action reaching it asks the views of the scene to bring it into focus.
 */
class SceneImageNode : public GraphNode, public SceneActionTarget, public ImageVisibleActionTarget, public GraphFocusable,
	public SceneImage
{
	public:

		SceneImageNode();

		GraphNodeType getType() override;

		virtual void accept(GraphNodeVisitor& visitor) override;

		void populateSurface(Handle<GraphHiveSceneSurface> surface) override;

		SceneActionTarget* getSceneActionTarget() override;

		ImageVisibleActionTarget* getImageVisibleActionTarget() override;

		FocusActionTarget* getFocusActionTarget() override;

		void setImageVisible(bool visible) override;

		unsigned getVersion() override;

	protected:

		// Ref counted.
		virtual ~SceneImageNode();

		void _poked(GraphPoke poke) override;

	private:

		// Do not allow copying.
		SceneImageNode(const SceneImageNode& copyFrom);
		SceneImageNode& operator= (const SceneImageNode& copyFrom);
};

#endif
