#include "ScriptAction.hpp"

#include "../actionTargets/ScriptActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

ScriptAction::~ScriptAction()
{
}

ScriptAction::ScriptAction(GraphHandle<GraphNode>& initNode, unsigned energy)
	: GraphAction(initNode, energy), _lastVisitedNode(0)
{
	// The script action is likely inherited, so leave the action flag as optional.
	// Any sub-class can always set it to required.
	_addFlag(SCRIPT_GRAPH_ACTION, false);
}

void ScriptAction::_apply(GraphNode* target)
{
	ScriptActionTarget* scriptTarget = target -> getScriptActionTarget();

	if(scriptTarget)
	{
		for(const auto& entry : _sharedGlobals)
		{
			const std::string& name = entry.first;
			const SharedValue& sharedValue = entry.second;

			switch(sharedValue.type)
			{
				case SharedValue::Type::BOOL:   scriptTarget -> setGlobal(name.c_str(), sharedValue.boolValue); break;
				case SharedValue::Type::INT:    scriptTarget -> setGlobal(name.c_str(), sharedValue.intValue); break;
				case SharedValue::Type::DOUBLE: scriptTarget -> setGlobal(name.c_str(), sharedValue.doubleValue); break;
				case SharedValue::Type::STRING: scriptTarget -> setGlobal(name.c_str(), sharedValue.stringValue.c_str()); break;
			}
		}

		scriptTarget -> invoke();

		_lastVisitedNode = GraphHandle<GraphNode>(target);
	}
}

bool ScriptAction::_starting()
{
	return true;
}

void ScriptAction::_complete()
{
}

void ScriptAction::_shareGlobal(const char* name, bool value)
{
	SharedValue& entry = _sharedGlobals[name];
	entry.type = SharedValue::Type::BOOL;
	entry.boolValue = value;
}

void ScriptAction::_shareGlobal(const char* name, int value)
{
	SharedValue& entry = _sharedGlobals[name];
	entry.type = SharedValue::Type::INT;
	entry.intValue = value;
}

void ScriptAction::_shareGlobal(const char* name, double value)
{
	SharedValue& entry = _sharedGlobals[name];
	entry.type = SharedValue::Type::DOUBLE;
	entry.doubleValue = value;
}

void ScriptAction::_shareGlobal(const char* name, const char* value)
{
	SharedValue& entry = _sharedGlobals[name];
	entry.type = SharedValue::Type::STRING;
	entry.stringValue = value;
}

bool ScriptAction::_getGlobal(const char* name, bool& value)
{
	if(_lastVisitedNode.isValid())
	{
		ScriptActionTarget* target = _lastVisitedNode.getInstance() -> getScriptActionTarget();
		if(target && target -> getGlobal(name, value)) return true;
	}

	auto it = _sharedGlobals.find(name);

	if(it != _sharedGlobals.end() && it -> second.type == SharedValue::Type::BOOL)
	{
		value = it -> second.boolValue;
		return true;
	}

	return false;
}

bool ScriptAction::_getGlobal(const char* name, int& value)
{
	if(_lastVisitedNode.isValid())
	{
		ScriptActionTarget* target = _lastVisitedNode.getInstance() -> getScriptActionTarget();
		if(target && target -> getGlobal(name, value)) return true;
	}

	auto it = _sharedGlobals.find(name);

	if(it != _sharedGlobals.end() && it -> second.type == SharedValue::Type::INT)
	{
		value = it -> second.intValue;
		return true;
	}

	return false;
}

bool ScriptAction::_getGlobal(const char* name, double& value)
{
	if(_lastVisitedNode.isValid())
	{
		ScriptActionTarget* target = _lastVisitedNode.getInstance() -> getScriptActionTarget();
		if(target && target -> getGlobal(name, value)) return true;
	}

	auto it = _sharedGlobals.find(name);

	if(it != _sharedGlobals.end() && it -> second.type == SharedValue::Type::DOUBLE)
	{
		value = it -> second.doubleValue;
		return true;
	}

	return false;
}

bool ScriptAction::_getGlobal(const char* name, const char*& value)
{
	if(_lastVisitedNode.isValid())
	{
		ScriptActionTarget* target = _lastVisitedNode.getInstance() -> getScriptActionTarget();
		if(target && target -> getGlobal(name, value)) return true;
	}

	auto it = _sharedGlobals.find(name);

	if(it != _sharedGlobals.end() && it -> second.type == SharedValue::Type::STRING)
	{
		value = it -> second.stringValue.c_str();
		return true;
	}

	return false;
}
