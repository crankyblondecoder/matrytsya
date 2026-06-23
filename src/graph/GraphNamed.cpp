#include "GraphNamed.hpp"

GraphNamed::~GraphNamed()
{
}

std::string GraphNamed::getName()
{
	return _name;
}

void GraphNamed::setName(std::string name)
{
	_name = name;

	if(_name.size() > GRAPH_HIVE_NAME_MAX_LEN) _name.resize(GRAPH_HIVE_NAME_MAX_LEN);
}

