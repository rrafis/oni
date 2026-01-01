#pragma once

#include <Platform.h>

#include "GameMath.h"

#include <entt/entt.hpp>

#include <unordered_map>
#include <vector>

namespace Engine
{
	struct FWorldPosition
	{
		glm::ivec3 Chunk;
		glm::vec2 Local;
	};

	enum class EObjectType : u32
	{
		Uninitialized = 0,
		Empty,
		Wall,
		StairUp,
		StairDown,
		WallTopLeft,
		WallTopRight,
		WallBottomLeft,
		WallBottomRight,
		WallHorizontalCenter,
		WallHorizontalTopLeft,
		WallHorizontalTopRight,
		WallHorizontalBottomLeft,
		WallHorizontalBottomRight,
		WallVerticalCenter,
		WallVerticalTopLeft,
		WallVerticalTopRight,
		WallVerticalBottomLeft,
		WallVerticalBottomRight,

		Invalid = UINT32_MAX
	};

	using FChunkStorage = std::vector<entt::handle>;
	using FChunkMap = std::unordered_map<glm::ivec3, FChunkStorage>;
	struct FWorld
	{
		glm::vec2 DrawPosition; // Upper left corner of the tile map
		glm::vec2 Padding;		// Padding for drawing
		f32 TileWidthInMeter;
		f32 TileWidthInPixels;
		f32 PixelsPerMeter;
		glm::uvec2 ChunkSize;

		glm::ivec2 ScreenTileDim;
		glm::ivec2 ScreenTileDimHalf;

		FChunkMap Chunks;
	};
} // namespace Engine
