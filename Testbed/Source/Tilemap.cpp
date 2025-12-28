#include "Tilemap.h"

#include "GameMath.h"
#include "Memory.h"

namespace re::Tilemap
{
	constexpr u32 ChunkShift = 4;
	constexpr u32 ChunkMask = (1u << ChunkShift) - 1u;

	inline auto GetChunkCoordinate(const FTilePosition& Tile) -> FChunkCoordinate
	{
		return FChunkCoordinate{ Tile.x >> ChunkShift, Tile.y >> ChunkShift, Tile.z };
	}

	inline auto GetTileCoordinate(const FTilePosition& Tile) -> FTileCoordinate
	{
		return FTileCoordinate{ Tile.x & ChunkMask, Tile.y & ChunkMask };
	}

	inline auto GetChunk(const FWorld& TileWorld, const FChunkCoordinate& ChunkCoord) -> FChunk*
	{
		if (ChunkCoord >= FChunkCoordinate(0) && ChunkCoord < TileWorld.WorldDimension)
		{
			return &TileWorld.Chunks[ChunkCoord.z * TileWorld.WorldDimension.y * TileWorld.WorldDimension.x + ChunkCoord.y * TileWorld.WorldDimension.x
				+ ChunkCoord.x];
		}

		return nullptr;
	}

	inline auto GetTileValue(const FWorld& TileWorld, const FChunk& Chunk, const FTileCoordinate& TileCoord) -> ETileType
	{
		if (TileCoord >= FTileCoordinate(0) && TileCoord < TileWorld.ChunkDimension)
		{
			return Chunk.Tiles[TileCoord.y * TileWorld.ChunkDimension.x + TileCoord.x];
		}

		return ETileType::Invalid;
	}

	inline auto GetTileValue(const FWorld& TileWorld, const FTilePosition& TilePos) -> ETileType
	{
		FChunk* Chunk = GetChunk(TileWorld, GetChunkCoordinate(TilePos));
		if (Chunk && Chunk->Tiles)
		{
			return GetTileValue(TileWorld, *Chunk, GetTileCoordinate(TilePos));
		}

		return ETileType::Invalid;
	}

	inline auto SetTileValue(const FWorld& TileWorld, const FChunk& Chunk, const FTileCoordinate& TileCoord, ETileType Value) -> void
	{
		if (TileCoord >= FTileCoordinate(0) && TileCoord < TileWorld.ChunkDimension)
		{
			Chunk.Tiles[TileCoord.y * TileWorld.ChunkDimension.x + TileCoord.x] = Value;
		}
	}

	inline auto SetTileValue(Memory::Arena::FMemoryArena& Arena, const FWorld& TileWorld, const FTilePosition& TilePos, ETileType Value) -> void
	{
		if (FChunk* Chunk = GetChunk(TileWorld, GetChunkCoordinate(TilePos)))
		{
			if (!Chunk->Tiles)
			{
				Chunk->Tiles = Memory::Arena::AllocateArray<Tilemap::ETileType>(Arena, TileWorld.ChunkDimension.x * TileWorld.ChunkDimension.y);
			}

			SetTileValue(TileWorld, *Chunk, GetTileCoordinate(TilePos), Value);
		}
	}

	inline auto IsTileEmpty(ETileType TileID)
	{
		return TileID == ETileType::Empty || TileID == ETileType::StairUp || TileID == ETileType::StairDown;
	}

	inline auto IsTilemapPointEmpty(const FWorld& TileWorld, const FTilemapPosition& Position) -> bool
	{
		auto TileID = GetTileValue(TileWorld, Position.Tile);
		return IsTileEmpty(TileID);
	}

	inline auto OnSameTile(const FWorld& TileWorld, const FTilemapPosition& Lhs, const FTilemapPosition& Rhs) -> bool
	{
		return GetTileValue(TileWorld, Lhs.Tile) == GetTileValue(TileWorld, Rhs.Tile);
	}

	inline auto Offset(const FWorld& TileWorld, const FTilemapPosition& Pos, const glm::vec2& Offset)
	{
		FTilemapPosition NewPosition = Pos;
		NewPosition.Local += Offset;
		auto TempOffset = glm::ivec2(glm::round(NewPosition.Local / (TileWorld.TileWidthInMeter)));
		NewPosition.Tile += FTilePosition(TempOffset, 0);
		NewPosition.Local -= glm::vec2(TempOffset) * TileWorld.TileWidthInMeter;

		return NewPosition;
	}

	inline auto RecalculateTilemapPosition(const FWorld& TileWorld, const FTilemapPosition& Position) -> FTilemapPosition
	{
		FTilemapPosition NewPosition = Position;
		auto Offset = glm::ivec2(glm::round(Position.Local / (TileWorld.TileWidthInMeter)));
		NewPosition.Tile += FTilePosition(Offset, 0);
		NewPosition.Local -= glm::vec2(Offset) * TileWorld.TileWidthInMeter;

		return NewPosition;
	}

	inline auto Subtract(const FWorld& TileWorld, const FTilemapPosition& Lhs, const FTilemapPosition& Rhs) -> glm::vec3
	{
		glm::vec2 DeltaXY;
		DeltaXY.x = (f32)Lhs.Tile.x - (f32)Rhs.Tile.x;
		DeltaXY.y = (f32)Lhs.Tile.y - (f32)Rhs.Tile.y;
		f32 DeltaZ = Lhs.Tile.z - Rhs.Tile.z;

		glm::vec3 Diff(DeltaXY * TileWorld.TileWidthInMeter + (Lhs.Local - Rhs.Local), DeltaZ * TileWorld.TileWidthInMeter);
		return Diff;
	}

	inline auto CreateTilemapPosition(f32 X, f32 Y, f32 Z) -> FTilemapPosition
	{
		FTilemapPosition Pos;
		Pos.Tile.x = X;
		Pos.Tile.y = Y;
		Pos.Tile.z = Z;
		Pos.Local = glm::vec2(0.0f);
		return Pos;
	}

	inline auto CreateTilemapPosition(glm::vec3 TilePos) -> FTilemapPosition
	{
		FTilemapPosition Pos;
		Pos.Tile = TilePos;
		Pos.Local = glm::vec2(0.0f);
		return Pos;
	}
} // namespace re::Tilemap
