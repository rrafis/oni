#pragma once

#include <Platform.h>

#include <glm/glm.hpp>

namespace re::Tilemap
{
	using FTilePosition = glm::uvec3;
	using FChunkCoordinate = glm::uvec3;
	using FTileCoordinate = glm::uvec2;

	struct FTilemapPosition
	{
		FTilePosition Tile;
		glm::vec2 Local;
	};

	enum class ETileType : u32
	{
		Uninitialized = 0,
		Empty = 1,
		Wall = 2,
		StairUp = 3,
		StairDown = 4,

		Invalid = UINT32_MAX
	};

	struct FChunk
	{
		ETileType* Tiles;
	};

	struct FWorld
	{
		FChunk* Chunks;
		glm::uvec3 WorldDimension;
		glm::uvec2 ChunkDimension;
		f32 TileWidthInMeter;
	};
} // namespace re::Tilemap
