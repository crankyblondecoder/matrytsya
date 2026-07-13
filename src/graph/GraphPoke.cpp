#include "GraphPoke.hpp"

GraphPoke::GraphPoke(PokeType type, std::array<int, 4> magnitudes) : _type{type}, _magnitudes{magnitudes}
{
}

GraphPoke::~GraphPoke()
{
}

GraphPoke::PokeType GraphPoke::getType()
{
	return _type;
}

std::array<int, 4> GraphPoke::getMagnitudes()
{
	return _magnitudes;
}
