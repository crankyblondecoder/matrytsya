#include "HiveRecorder.hpp"

#include "HiveLoader.hpp"
#include "HiveNodeCapture.hpp"
#include "PersistException.hpp"
#include "../graph/GraphHive.hpp"
#include "../graph/GraphNode.hpp"
#include "../log/log.hpp"
#include "../thread/thread.hpp"
#include "../util/CastHandle.hpp"
#include "../util/EventEmitter.hpp"

#include <algorithm>

HiveRecorder::HiveRecorder(Handle<GraphHive> hive, HiveLoader& loader) : _hive(hive)
{
	_hiveName = _hive.getInstance() -> getName();

	for(unsigned i = 0; i < loader.getNodeCount(); i++)
	{
		HiveNodeDescriptor descriptor = loader.getNode(i);

		Handle<GraphNode> node = _hive.getInstance() -> getNode(descriptor.name);

		_nodeOrder.push_back(descriptor.name);
		_nodes.emplace(descriptor.name, descriptor);

		if(node.isValid()) _nodeIds[descriptor.name] = node.getInstance() -> getId();
	}

	for(unsigned i = 0; i < loader.getSurfaceCount(); i++) _surfaces.push_back(loader.getSurface(i));
}

HiveRecorder::~HiveRecorder()
{
}

void HiveRecorder::start()
{
	{ SYNC(_lock)

		if(_listening) return;

		_listening = true;
	}

	_hive.getInstance() -> addListener(*this);
}

void HiveRecorder::stop()
{
	{ SYNC(_lock)

		if(!_listening) return;

		_listening = false;
	}

	_hive.getInstance() -> removeListener(*this);
}

std::string HiveRecorder::getHiveName()
{
	return _hiveName;
}

HiveSnapshot HiveRecorder::snapshot()
{
	HiveSnapshot snapshot;

	snapshot.name = _hiveName;

	__captureNewNodes();
	__forgetRemovedNodes();

	std::vector<std::string> nodeOrder;

	{ SYNC(_lock)

		nodeOrder = _nodeOrder;
	}

	for(const std::string& name : nodeOrder)
	{
		HiveNodeDescriptor descriptor;

		if(__liveNode(name, descriptor)) snapshot.nodes.push_back(descriptor);
	}

	snapshot.surfaces = _surfaces;

	for(GraphHive::StrobeRegistration& emitter : _hive.getInstance() -> getStrobeEmitters())
	{
		snapshot.strobeEmitters.push_back(HiveStrobeDescriptor{emitter.name, emitter.periodMs});
	}

	for(GraphHive::StrobeRegistration& surface : _hive.getInstance() -> getStrobeSurfaces())
	{
		snapshot.strobeSurfaces.push_back(HiveStrobeDescriptor{surface.name, surface.periodMs});
	}

	return snapshot;
}

bool HiveRecorder::getNode(const std::string& nodeName, HiveNodeDescriptor& descriptor)
{
	__captureNewNodes();

	return __liveNode(nodeName, descriptor);
}

void HiveRecorder::hiveNodeAdded(EventEmitter<GraphHiveListener>& emitter)
{
	// Runs on whichever thread added the node, which may be a builder's script session, so nothing may escape.
	try
	{
		__captureNewNodes();
	}
	catch(PersistException& exception)
	{
		LOG(Logger::ERROR, "Could not record a node added to hive '" + _hiveName + "': " +
			PersistException::getErrorDescription(exception.getError()))
	}
	catch(...)
	{
		LOG(Logger::ERROR, "Could not record a node added to hive '" + _hiveName + "'")
	}
}

void HiveRecorder::hiveNodeRemoved(EventEmitter<GraphHiveListener>& emitter)
{
	__forgetRemovedNodes();
}

void HiveRecorder::populateEventListenerHandle(CastHandle<GraphHiveListener>& handle)
{
	handle = CastHandle<GraphHiveListener>(this, this);
}

void HiveRecorder::__captureNewNodes()
{
	std::vector<Handle<GraphNode>> toCapture;

	for(const std::string& name : _hive.getInstance() -> getNodeNames())
	{
		Handle<GraphNode> node = _hive.getInstance() -> getNode(name);

		// Gone again already, so there is nothing to capture.
		if(!node.isValid()) continue;

		{ SYNC(_lock)

			std::map<std::string, unsigned>::iterator recordedId = _nodeIds.find(name);

			if(recordedId == _nodeIds.end() || recordedId -> second != node.getInstance() -> getId())
			{
				toCapture.push_back(node);
			}
		}
	}

	for(Handle<GraphNode>& node : toCapture)
	{
		std::string name = node.getInstance() -> getName();
		unsigned id = node.getInstance() -> getId();

		HiveNodeDescriptor descriptor = HiveNodeCapture::capture(*node.getInstance());

		{ SYNC(_lock)

			std::map<std::string, unsigned>::iterator recordedId = _nodeIds.find(name);

			// Another thread may have captured it in the meantime, in which case its capture, being the earlier
			// or as early, is kept.
			if(recordedId != _nodeIds.end() && recordedId -> second == id) continue;

			// A node replacing one of the same name keeps the place of the one it replaced.
			if(_nodes.find(name) == _nodes.end()) _nodeOrder.push_back(name);

			_nodes[name] = descriptor;
			_nodeIds[name] = id;
		}
	}
}

void HiveRecorder::__forgetRemovedNodes()
{
	std::vector<std::string> recordedNames;

	{ SYNC(_lock)

		recordedNames = _nodeOrder;
	}

	std::vector<std::string> removedNames;

	for(const std::string& name : recordedNames)
	{
		if(!_hive.getInstance() -> getNode(name).isValid()) removedNames.push_back(name);
	}

	// The hive holds no nodes once it is shut down, which is not every node having been removed: keeping the
	// records then means a snapshot taken afterwards is simply empty, rather than the records being lost.
	if(removedNames.size() == recordedNames.size()) return;

	{ SYNC(_lock)

		for(const std::string& name : removedNames)
		{
			_nodes.erase(name);
			_nodeIds.erase(name);
			_nodeOrder.erase(std::remove(_nodeOrder.begin(), _nodeOrder.end(), name), _nodeOrder.end());
		}
	}
}

bool HiveRecorder::__liveNode(const std::string& nodeName, HiveNodeDescriptor& descriptor)
{
	Handle<GraphNode> node = _hive.getInstance() -> getNode(nodeName);

	if(!node.isValid()) return false;

	{ SYNC(_lock)

		std::map<std::string, HiveNodeDescriptor>::iterator recorded = _nodes.find(nodeName);

		if(recorded == _nodes.end()) return false;

		descriptor = recorded -> second;
	}

	// Edges and build state are what a hive growing changes, so are given as they stand rather than as the node
	// joined.
	HiveNodeDescriptor live = HiveNodeCapture::capture(*node.getInstance());

	descriptor.edges = live.edges;
	descriptor.built = live.built;

	return true;
}
