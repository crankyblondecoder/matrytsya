#include "GraphHive.hpp"

#include "GraphHiveHandle.hpp"

GraphHiveHandle::GraphHiveHandle(GraphHive* node)
{
	if(node && node -> incrRef())
	{
		_referencedHive = node;
	}
	else
	{
		_referencedHive = 0;
	}
}

GraphHiveHandle::GraphHiveHandle(const GraphHiveHandle& copyFrom)
{
	if(copyFrom._referencedHive -> incrRef())
	{
		_referencedHive = copyFrom._referencedHive;
	}
	else
	{
		_referencedHive = 0;
	}
}

GraphHiveHandle& GraphHiveHandle::operator= (const GraphHiveHandle& copyFrom)
{
	if(_referencedHive) _referencedHive -> decrRef();

	if(copyFrom._referencedHive -> incrRef())
	{
		_referencedHive = copyFrom._referencedHive;
	}
	else
	{
		_referencedHive = 0;
	}

	return *this;
}

GraphHiveHandle::~GraphHiveHandle()
{
	if(_referencedHive) _referencedHive -> decrRef();

	_referencedHive = 0;
}

GraphHive* GraphHiveHandle::getHive()
{
	if(_referencedHive -> incrRef())
	{
		return _referencedHive;
	}

	return 0;
}

bool GraphHiveHandle::isValid()
{
	return _referencedHive != 0;
}

