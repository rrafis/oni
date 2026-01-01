#include "World.h"

#include "GameMath.h"
#include "Memory.h"

#include <entt/entt.hpp>

namespace Engine
{
	inline auto Offset(FWorld& World, const FWorldPosition& Pos, const glm::vec2& Offset) -> FWorldPosition
	{
		auto ChunkDim = glm::vec2(World.ChunkSize) * World.TileWidthInMeter;

		FWorldPosition NewPosition = Pos;
		NewPosition.Local += Offset;
		auto TempOffset = glm::ivec2(glm::floor(NewPosition.Local / ChunkDim));
		NewPosition.Chunk += glm::ivec3(TempOffset, 0);
		NewPosition.Local -= glm::vec2(TempOffset) * ChunkDim;

		return NewPosition;
	}

	inline auto Subtract(FWorld& World, const FWorldPosition& Lhs, const FWorldPosition& Rhs) -> glm::vec3
	{
		auto ChunkDim = glm::vec2(World.ChunkSize) * World.TileWidthInMeter;

		glm::vec2 DeltaXY;
		DeltaXY.x = Lhs.Chunk.x - Rhs.Chunk.x;
		DeltaXY.y = Lhs.Chunk.y - Rhs.Chunk.y;
		f32 DeltaZ = Lhs.Chunk.z - Rhs.Chunk.z;

		glm::vec3 Diff(DeltaXY * ChunkDim + (Lhs.Local - Rhs.Local), DeltaZ);
		return Diff;
	}

	inline auto CreateWorldPosition(FWorld& World, glm::ivec3 TilePos) -> FWorldPosition
	{
		auto ChunkPos = glm::ivec2(glm::floor(glm::vec2(TilePos) / glm::vec2(World.ChunkSize)));
		auto LocalPos = glm::vec2(glm::ivec2(TilePos) - glm::ivec2(ChunkPos * glm::ivec2(World.ChunkSize)));
		auto ChunkOffset = LocalPos * World.TileWidthInMeter;

		FWorldPosition Pos;
		Pos.Chunk = glm::ivec3(ChunkPos, TilePos.z);
		Pos.Local = ChunkOffset;
		return Pos;
	}

	inline auto CreateWorldPosition(FWorld& World, i32 X, i32 Y, i32 Z) -> FWorldPosition
	{
		return CreateWorldPosition(World, glm::ivec3(X, Y, Z));
	}

	inline auto MapToCameraSpace(FWorld& World, const FWorldPosition& CameraPos, const FWorldPosition& WorldPosition, glm::vec3& Position) -> void
	{
		auto Diff = Subtract(World, WorldPosition, CameraPos);
		Position = glm::vec3(Diff.x, Diff.y, WorldPosition.Chunk.z);
	}

	inline auto MapToChunkSpace(FWorld& World, const FWorldPosition& CameraPos, const glm::vec3& Position, FWorldPosition& WorldPosition) -> void
	{
		auto ChunkZ = WorldPosition.Chunk.z;
		WorldPosition = Offset(World, CameraPos, Position);
		WorldPosition.Chunk.z = ChunkZ;
	}

	inline auto OnSameChunk(const FWorld& World, const FWorldPosition& Old, const FWorldPosition& New) -> bool
	{
		return Old.Chunk == New.Chunk;
	}

	inline auto SetLocation(FWorld& World, entt::handle Entity, FWorldPosition& Old, const FWorldPosition& New) -> void
	{
		Old = New;
		auto& ChunkStorage = World.Chunks[New.Chunk];
		ChunkStorage.push_back(Entity);
	}

	inline auto ChangeLocation(FWorld& World, entt::handle Entity, FWorldPosition& Old, const FWorldPosition& New) -> void
	{
		if (!OnSameChunk(World, Old, New))
		{
			if (World.Chunks.contains(Old.Chunk))
			{
				auto& ChunkStorage = World.Chunks[Old.Chunk];
				auto result = std::remove(ChunkStorage.begin(), ChunkStorage.end(), Entity);
			}

			SetLocation(World, Entity, Old, New);
		}
	}

	inline auto GetChunkStorage(FWorld& World, glm::ivec3 Chunk) -> FChunkStorage*
	{
		if (World.Chunks.contains(Chunk))
			return &World.Chunks[Chunk];
		return nullptr;
	}
} // namespace Engine
