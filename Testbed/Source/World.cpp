#include "World.h"

#include "GameMath.h"
#include "Memory.h"

namespace Engine
{
	inline auto Offset(f32 TileWidthInMeter, const FWorldPosition& Pos, const glm::vec2& Offset)
	{
		FWorldPosition NewPosition = Pos;
		NewPosition.Local += Offset;
		auto TempOffset = glm::ivec2(glm::round(NewPosition.Local / TileWidthInMeter));
		NewPosition.Tile += glm::ivec3(TempOffset, 0);
		NewPosition.Local -= glm::vec2(TempOffset) * TileWidthInMeter;

		return NewPosition;
	}

	inline auto Subtract(f32 TileWidthInMeter, const FWorldPosition& Lhs, const FWorldPosition& Rhs) -> glm::vec3
	{
		glm::vec2 DeltaXY;
		DeltaXY.x = (f32)Lhs.Tile.x - (f32)Rhs.Tile.x;
		DeltaXY.y = (f32)Lhs.Tile.y - (f32)Rhs.Tile.y;
		f32 DeltaZ = Lhs.Tile.z - Rhs.Tile.z;

		glm::vec3 Diff(DeltaXY * TileWidthInMeter + (Lhs.Local - Rhs.Local), DeltaZ * TileWidthInMeter);
		return Diff;
	}

	inline auto CreateTilemapPosition(f32 X, f32 Y, f32 Z) -> FWorldPosition
	{
		FWorldPosition Pos;
		Pos.Tile.x = X;
		Pos.Tile.y = Y;
		Pos.Tile.z = Z;
		Pos.Local = glm::vec2(0.0f);
		return Pos;
	}

	inline auto CreateTilemapPosition(glm::vec3 TilePos) -> FWorldPosition
	{
		FWorldPosition Pos;
		Pos.Tile = TilePos;
		Pos.Local = glm::vec2(0.0f);
		return Pos;
	}

	auto MapToCameraSpace(f32 TileWidthInMeter, const FWorldPosition& CameraPos, const FWorldPosition& DormantPos, glm::vec3& Position)
	{
		auto Diff = Subtract(TileWidthInMeter, DormantPos, CameraPos);
		Position = glm::vec3(Diff.x, Diff.y, Position.z);
	}

	auto MapToTileSpace(f32 TileWidthInMeter, const FWorldPosition& CameraPos, const glm::vec3& Position, FWorldPosition& DormantPos)
	{
		DormantPos = Offset(TileWidthInMeter, CameraPos, Position);
		DormantPos.Tile.z = Position.z;
	}
} // namespace Engine
