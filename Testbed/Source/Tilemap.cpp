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

	inline auto IsTilemapPointEmpty(const FWorld& TileWorld, const FTilemapPosition& Position) -> bool
	{
		auto TileID = GetTileValue(TileWorld, Position.Tile);
		return TileID == ETileType::Empty || TileID == ETileType::StairUp || TileID == ETileType::StairDown;
	}

	inline auto OnSameTile(const FWorld& TileWorld, const FTilemapPosition& Lhs, const FTilemapPosition& Rhs) -> bool
	{
		return GetTileValue(TileWorld, Lhs.Tile) == GetTileValue(TileWorld, Rhs.Tile);
	}

	inline auto RecalculateTilemapPosition(const FWorld& TileWorld, const FTilemapPosition& Position) -> FTilemapPosition
	{
		FTilemapPosition NewPosition = Position;
		auto Offset = glm::ivec2(glm::round(Position.Local / (TileWorld.TileWidthInMeter)));
		NewPosition.Tile += FTilePosition(Offset, 0);
		NewPosition.Local -= glm::vec2(Offset) * TileWorld.TileWidthInMeter;

		return NewPosition;
	}
} // namespace re::Tilemap
