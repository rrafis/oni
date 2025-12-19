#include "Tilemap.h"

#include "GameMath.h"
#include "Memory.h"

namespace re::Tilemap
{
	constexpr u32 ChunkShift = 4;
	constexpr u32 ChunkMask = (1u << ChunkShift) - 1u;

	inline auto GetChunkCoordinate(const FTilePosition& Tile) -> FChunkCoordinate
	{
		return FChunkCoordinate{ Tile.x >> ChunkShift, Tile.y >> ChunkShift };
	}

	inline auto GetTileCoordinate(const FTilePosition& Tile) -> FTileCoordinate
	{
		return FTileCoordinate{ Tile.x & ChunkMask, Tile.y & ChunkMask };
	}

	inline auto GetChunk(const FWorld& TileWorld, const FChunkCoordinate& ChunkCoord) -> FChunk*
	{
		if (ChunkCoord >= FChunkCoordinate(0) && ChunkCoord < TileWorld.WorldDimension)
		{
			return &TileWorld.Chunks[ChunkCoord.y * TileWorld.WorldDimension.x + ChunkCoord.x];
		}

		return nullptr;
	}

	inline auto GetTileValue(const FWorld& TileWorld, const FChunk& Chunk, const FTileCoordinate& TileCoord) -> u32
	{
		if (TileCoord >= FTileCoordinate(0) && TileCoord < TileWorld.ChunkDimension)
		{
			return Chunk.Tiles[TileCoord.y * TileWorld.ChunkDimension.x + TileCoord.x];
		}

		return 0;
	}

	inline auto GetTileValue(const FWorld& TileWorld, const FTilePosition& TilePos) -> u32
	{
		if (FChunk* Chunk = GetChunk(TileWorld, GetChunkCoordinate(TilePos)))
		{
			return GetTileValue(TileWorld, *Chunk, GetTileCoordinate(TilePos));
		}

		return 0;
	}

	inline auto SetTileValue(const FWorld& TileWorld, const FChunk& Chunk, const FTileCoordinate& TileCoord, u32 Value) -> void
	{
		if (TileCoord >= FTileCoordinate(0) && TileCoord < TileWorld.ChunkDimension)
			Chunk.Tiles[TileCoord.y * TileWorld.ChunkDimension.x + TileCoord.x] = Value;
	}

	inline auto SetTileValue(Memory::Arena::FMemoryArena& Arena, const FWorld& TileWorld, const FTilePosition& TilePos, u32 Value) -> void
	{
		FChunk* Chunk = GetChunk(TileWorld, GetChunkCoordinate(TilePos));
		if (!Chunk)
		{
		}

		if (Chunk)
			SetTileValue(TileWorld, *Chunk, GetTileCoordinate(TilePos), Value);
	}

	inline auto IsTilemapPointEmpty(const FWorld& TileWorld, const FTilemapPosition& Position) -> bool
	{
		return GetTileValue(TileWorld, Position.Tile) == 0;
	}

	inline auto RecalculateTilemapPosition(const FWorld& TileWorld, const FTilemapPosition& Position) -> FTilemapPosition
	{
		FTilemapPosition NewPosition = Position;
		auto Offset = glm::ivec2(glm::round(Position.Local / (TileWorld.TileWidthInMeter)));
		NewPosition.Tile += Offset;
		NewPosition.Local -= glm::vec2(Offset) * TileWorld.TileWidthInMeter;

		return NewPosition;
	}
} // namespace re::Tilemap
