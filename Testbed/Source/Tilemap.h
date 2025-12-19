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

	struct FChunk
	{
		u32* Tiles;
	};

	struct FWorld
	{
		u32 ChunkShift;
		u32 ChunkMask;

		glm::uvec2 WorldDimension;
		glm::uvec2 ChunkDimension;

		FChunk* Chunks;

		f32 TileWidthInMeter;
		f32 TileWidthInPixels;
		f32 PixelsPerMeter;
	};
} // namespace re::Tilemap
