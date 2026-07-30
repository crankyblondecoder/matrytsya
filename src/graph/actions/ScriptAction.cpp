#include "ScriptAction.hpp"

#include "../actionTargets/ScriptActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"
#include "../nodes/ScriptSession.hpp"

#include "../../thread/ThreadException.hpp"

ScriptAction::~ScriptAction()
{
}

ScriptAction::ScriptAction(Handle<GraphNode>& initNode, unsigned energy)
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
		try
		{
			// One session, so the shared globals and the run that reads them cannot be interleaved with
			// another action being applied to the same node.
			{ Handle<ScriptSession> sessionHandle = scriptTarget -> requestCoreSession();

				ScriptSession* session = sessionHandle.getInstance();

				for(const auto& entry : _sharedGlobals)
				{
					const std::string& name = entry.first;
					const SharedValue& sharedValue = entry.second;

					switch(sharedValue.type)
					{
						case SharedValue::Type::BOOL:   session -> setGlobal(name.c_str(), sharedValue.boolValue); break;
						case SharedValue::Type::INT:    session -> setGlobal(name.c_str(), sharedValue.intValue); break;
						case SharedValue::Type::DOUBLE: session -> setGlobal(name.c_str(), sharedValue.doubleValue); break;
						case SharedValue::Type::STRING: session -> setGlobal(name.c_str(), sharedValue.stringValue.c_str()); break;
					}
				}

				session -> run();
			}
		}
		catch(ThreadException& ex)
		{
			// Nothing may escape into the work cycle: an exception out of here strands the action, as the
			// traversal and completion that follow this call would never run. A node whose state could not be
			// claimed is simply left unvisited.
			return;
		}

		// Assigned outside the session so that releasing the previously visited node, which can delete it,
		// never happens while a state lock is held.
		_lastVisitedNode = Handle<GraphNode>(target);
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

Handle<ScriptSession> ScriptAction::__requestLastVisitedSession()
{
	if(_lastVisitedNode.isValid())
	{
		ScriptActionTarget* target = _lastVisitedNode.getInstance() -> getScriptActionTarget();

		if(target)
		{
			try
			{
				return target -> requestCoreSession();
			}
			catch(ThreadException& ex)
			{
				// A state that can't be claimed reads the same way as a node that never set the global.
			}
		}
	}

	return Handle<ScriptSession>(0);
}

bool ScriptAction::_getGlobal(const char* name, bool& value)
{
	{ Handle<ScriptSession> sessionHandle = __requestLastVisitedSession();

		if(sessionHandle.isValid() && sessionHandle.getInstance() -> getGlobal(name, value)) return true;
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
	{ Handle<ScriptSession> sessionHandle = __requestLastVisitedSession();

		if(sessionHandle.isValid() && sessionHandle.getInstance() -> getGlobal(name, value)) return true;
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
	{ Handle<ScriptSession> sessionHandle = __requestLastVisitedSession();

		if(sessionHandle.isValid() && sessionHandle.getInstance() -> getGlobal(name, value)) return true;
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
	{ Handle<ScriptSession> sessionHandle = __requestLastVisitedSession();

		if(sessionHandle.isValid() && sessionHandle.getInstance() -> getGlobal(name, value)) return true;
	}

	auto it = _sharedGlobals.find(name);

	if(it != _sharedGlobals.end() && it -> second.type == SharedValue::Type::STRING)
	{
		value = it -> second.stringValue.c_str();
		return true;
	}

	return false;
}
