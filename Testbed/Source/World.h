#pragma once

#include <Platform.h>

#include "GameMath.h"

namespace Engine
{
	struct FWorldPosition
	{
		glm::ivec3 Tile;
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

	struct FWorld
	{
		glm::vec2 DrawPosition; // Upper left corner of the tile map
		glm::vec2 Padding;		// Padding for drawing
		f32 TileWidthInMeter;
		f32 TileWidthInPixels;
		f32 PixelsPerMeter;

		glm::ivec2 ScreenTileDim;
		glm::ivec2 ScreenTileDimHalf;
	};
} // namespace Engine
