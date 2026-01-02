#pragma once

#include <Platform.h>

#include "GameMath.h"
#include "World.h"

struct FImage
{
	u32 Width, Height;
	u8 Channels;
	u32* Pixels;
};

struct FSprite
{
	FImage* Texture;
	glm::ivec2 Pivot;
	glm::uvec2 Min;
	glm::uvec2 Max;
};

namespace Comp
{
	struct FPlayer
	{
		u32 ControllerIndex;
	};

	struct FTransform
	{
		glm::vec3 Position;
	};

	struct FMovement
	{
		glm::vec2 Velocity;
		f32 PlayerAccelerationRate;
	};

	struct FActiveTag
	{
		glm::ivec3 Chunk;
	};

	struct FSize
	{
		glm::vec2 Size;
	};

	struct FDrawSprite
	{
		FSprite Sprite;
	};

	struct FWorldTransform
	{
		Engine::FWorldPosition Position;
	};

	struct FTeleport
	{
		i32 Z;
	};

	struct FCollider
	{
		glm::vec2 Size;
		bool bCollide;
	};

	constexpr u64 SizeOfComponents = sizeof(FPlayer) + sizeof(FTransform) + sizeof(FMovement) + sizeof(FSize) + sizeof(FDrawSprite) + sizeof(FWorldTransform)
		+ sizeof(FTeleport) + sizeof(FCollider);
} // namespace Comp
