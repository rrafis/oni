#pragma once

#include <Platform.h>

#include <glm/glm.hpp>

namespace re::Tilemap
{
	using FTilePosition = glm::uvec2;
	using FChunkCoordinate = glm::uvec2;
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

		Invalid = UINT32_MAX
	};

	struct FChunk
	{
		ETileType* Tiles;
	};

	struct FWorld
	{
		FChunk* Chunks;
		glm::uvec2 WorldDimension;
		glm::uvec2 ChunkDimension;
		f32 TileWidthInMeter;
	};
} // namespace re::Tilemap
