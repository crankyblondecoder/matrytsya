#include "GraphPoke.hpp"

GraphPoke::GraphPoke(PokeType type, PokeData data, unsigned chunkId) : _type{type}, _data{data}, _chunkId{chunkId}
{
}

GraphPoke::~GraphPoke()
{
}

GraphPoke::PokeType GraphPoke::getType()
{
	return _type;
}

int GraphPoke::getHitDuration()
{
	return _data.hitDuration;
}

void GraphPoke::getDragVector(float vectorToPopulate[3])
{
	vectorToPopulate[0] = _data.dragVector[0];
	vectorToPopulate[1] = _data.dragVector[1];
	vectorToPopulate[2] = _data.dragVector[2];
}

unsigned GraphPoke::getChunkId()
{
	return _chunkId;
}

