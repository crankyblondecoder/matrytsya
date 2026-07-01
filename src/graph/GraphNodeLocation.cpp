#include "GraphNodeLocation.hpp"

GraphNodeLocation::GraphNodeLocation()
{
}

GraphNodeLocation::GraphNodeLocation(const GraphNodeLocation& copyFrom)
	: _hostname{copyFrom._hostname},
	  _port{copyFrom._port},
	  _hiveName{copyFrom._hiveName},
	  _nodeName{copyFrom._nodeName}
{
}

GraphNodeLocation& GraphNodeLocation::operator= (const GraphNodeLocation& copyFrom)
{
	_hostname = copyFrom._hostname;
	_port = copyFrom._port;
	_hiveName = copyFrom._hiveName;
	_nodeName = copyFrom._nodeName;
	return *this;
}

GraphNodeLocation::~GraphNodeLocation()
{
}

bool GraphNodeLocation::isLocalHost()
{
	return _hostname == "localhost" || _hostname == "127.0.0.1" || _hostname == "::1";
}

std::string GraphNodeLocation::getHostname()
{
	return _hostname;
}

void GraphNodeLocation::setHostname(std::string hostname)
{
	_hostname = hostname;
}

int GraphNodeLocation::getPort()
{
	return _port;
}

void GraphNodeLocation::setPort(int port)
{
	_port = port;
}

std::string GraphNodeLocation::getHiveName()
{
	return _hiveName;
}

void GraphNodeLocation::setHiveName(std::string hiveName)
{
	_hiveName = hiveName;
}

std::string GraphNodeLocation::getNodeName()
{
	return _nodeName;
}

void GraphNodeLocation::setNodeName(std::string nodeName)
{
	_nodeName = nodeName;
}
