#include "SceneAction.hpp"

#include "../actionTargets/SceneActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"
#include "../GraphNode.hpp"

SceneAction::~SceneAction()
{
}

SceneAction::SceneAction(Handle<GraphNode> initNode, Handle<GraphHiveSceneSurface> surface, unsigned energy)
	: GraphAction(initNode, energy, 2), _surface(surface)
{
	_addFlag(SCENE_GRAPH_ACTION, true);
}

bool SceneAction::_apply(GraphNode* target)
{
	SceneActionTarget* sceneTarget = target -> getSceneActionTarget();

	if(sceneTarget)
	{
		// First pass always calculates the scenes version, which is used to determine if the surface should be
		// populated.

		if(_getCurrentPassNum() == 1)
		{
			// Each node's id is folded in with its version, rather than the versions simply being summed, so that
			// replacing a node with one that happens to reach the same version (an edited node rebuilt from the same
			// script, say) still changes the scene version and the surface is repopulated with the replacement.
			unsigned nodeVersion = target -> getId() * 2654435761u + sceneTarget -> getVersion();

			_version = (_version.load() * 16777619u) ^ nodeVersion;
		}
		else
		{
			if(_surface.isValid()) sceneTarget -> populateSurface(_surface);
		}
	}

	return false;
}

bool SceneAction::_starting()
{
	return true;
}

bool SceneAction::_processNextPass(unsigned currentPassNum)
{
	_populatingSurface = false;

	if(_surface.isValid() && _surface.getInstance() -> getPopulateVersion() != _version)
	{
		_populatingSurface = _surface.getInstance() -> populateStart(_version);
	}

	return _populatingSurface;
}

void SceneAction::_complete()
{
	if(_populatingSurface && _surface.isValid())
	{
		_surface.getInstance() -> populateEnd();
	}
}

unsigned SceneAction::getSceneVersion()
{
	return _version;
}
