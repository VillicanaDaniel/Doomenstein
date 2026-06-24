#pragma once
#include "Engine/Math/IntVec2.hpp"

class TileDefinition;

class Tile
{
public:
	TileDefinition const* m_tileDef = nullptr;
	IntVec2 m_tileCoords = IntVec2::ZERO;
};