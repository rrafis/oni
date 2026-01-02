#pragma once

#include <Platform.h>

#include "GameMath.h"
#include "Memory.h"

#include <entt/entt.hpp>

#include <unordered_map>
#include <unordered_set>

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

	struct HandleHash
	{
		std::size_t operator()(const entt::handle& h) const noexcept
		{
			// Combine registry pointer and entity id
			std::size_t seed = 0;
			auto reg_ptr = reinterpret_cast<std::uintptr_t>(h.registry());
			auto ent_val = static_cast<std::uint32_t>(h.entity());

			// Simple hash combine
			seed ^= std::hash<std::uintptr_t>{}(reg_ptr) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<std::uint32_t>{}(ent_val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

			return seed;
		}
	};

	struct HandleEqual
	{
		bool operator()(const entt::handle& a, const entt::handle& b) const noexcept
		{
			return a.registry() == b.registry() && a.entity() == b.entity();
		}
	};
	using FChunkStorage = std::unordered_set<entt::handle, HandleHash, HandleEqual>;
	using FChunkMap = std::unordered_map<glm::ivec3, FChunkStorage>;
	struct FWorld
	{
		glm::vec2 DrawPosition; // Upper left corner of the tile map
		f32 TileWidthInMeter;
		f32 TileWidthInPixels;
		f32 PixelsPerMeter;
		glm::uvec2 ChunkSize;

		glm::ivec2 ScreenTileDim;
		glm::ivec2 ScreenTileDimHalf;

		FChunkMap Chunks;
	};

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
		auto ChunkZ = Position.z;
		WorldPosition = Offset(World, CameraPos, Position);
		WorldPosition.Chunk.z = ChunkZ;
	}

	inline auto OnSameChunk(const FWorld& World, const glm::ivec3& Old, const glm::ivec3& New) -> bool
	{
		return Old == New;
	}

	inline auto SetLocation(FWorld& World, entt::handle Entity, const glm::ivec3& New) -> void
	{
		auto& ChunkStorage = World.Chunks[New];
		ChunkStorage.emplace(Entity);
	}

	inline auto ChangeLocation(FWorld& World, entt::handle Entity, const glm::ivec3& Old, const glm::ivec3& New) -> void
	{
		if (!OnSameChunk(World, Old, New))
		{
			if (World.Chunks.contains(Old))
			{
				auto& ChunkStorage = World.Chunks[Old];
				ChunkStorage.erase(Entity);
			}

			SetLocation(World, Entity, New);
		}
	}

	inline auto GetChunkStorage(FWorld& World, glm::ivec3 Chunk) -> FChunkStorage*
	{
		if (World.Chunks.contains(Chunk))
			return &World.Chunks[Chunk];
		return nullptr;
	}
} // namespace Engine
