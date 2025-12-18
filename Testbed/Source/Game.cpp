#include <Platform.h>

#include "GameMath.h"

struct FChunkPosition
{
	glm::uvec2 Chunk;
	glm::uvec2 Tile;
};

struct FWorldPosition
{
	glm::uvec2 Tile;
	glm::vec2 Local;
};

struct FGameState
{
	f32 xOffset;
	f32 yOffset;
	f32 OffsetSpeed;

	FWorldPosition PlayerPosition;
	glm::vec2 PlayerSize;
	glm::vec2 PlayerSpeed;
};

struct FTileChunk
{
	u32* Tiles;
};

struct FTileWorld
{
	u32 ChunkShift;
	u32 ChunkMask;

	glm::uvec2 WorldDimension;

	glm::uvec2 TilemapDimension;
	FTileChunk* Tilemaps;

	glm::vec2 WorldPosition; // Upper left corner of the tile map
	glm::vec2 Padding;		 // Padding for drawing

	f32 TileWidthInMeter;
	f32 TileWidthInPixels;
	f32 PixelsPerMeter;
};

constexpr u32 s_TileMapX = 256;
constexpr u32 s_TileMapY = 256;

static u32 s_TileMap[s_TileMapY][s_TileMapX] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, //
	{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1 },
	{ 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1 },
	{ 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
	{ 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 }, //
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

RE_API auto GameGetSurfaceDesc() -> FGameSurfaceDesc*
{
	static FGameSurfaceDesc Props;
	Props.Width = 1200.f;
	Props.Height = 800.f;
	Props.bCenterd = true;
	Props.Title = "Testbed Window";

	return &Props;
}

RE_API auto GameInitialize(FGameMemory& Memory) -> void
{
	// TODO(Rafis): Assert Memory is valid

	FGameState& State = *(FGameState*)Memory.PermanentStorage;
	State.xOffset = 0;
	State.yOffset = 0;
	State.OffsetSpeed = 2 * 255.f;

	State.PlayerSpeed = glm::vec2{ 6.0f };
	State.PlayerSize = glm::vec2{ 1.0f };

	State.PlayerPosition.Tile = glm::uvec2(5, 1);
	State.PlayerPosition.Local = glm::vec2(0.0f, 0.0f);
}

inline auto GetTileValue(const FTileWorld& World, const FTileChunk& TileMap, glm::uvec2 Tile) -> u32
{
	if (Tile >= glm::uvec2(0) && Tile < World.TilemapDimension)
		return TileMap.Tiles[Tile.y * World.TilemapDimension.x + Tile.x];

	return 0;
}

inline auto GetChunk(const FTileWorld& World, const glm::uvec2& TileChunk) -> FTileChunk*
{
	if (TileChunk >= glm::uvec2(0) && TileChunk < World.WorldDimension)
		return &World.Tilemaps[TileChunk.y * World.WorldDimension.x + TileChunk.x];

	return nullptr;
}

inline auto CalculateWorldPosition(const FTileWorld& World, const FWorldPosition& Position) -> FWorldPosition
{
	FWorldPosition NewPosition = Position;
	auto Offset = glm::ivec2(glm::round(Position.Local / (World.TileWidthInMeter)));
	NewPosition.Tile += Offset;
	NewPosition.Local -= glm::vec2(Offset) * World.TileWidthInMeter;

	return NewPosition;
}

inline auto GetChunkPosition(const FTileWorld& World, const glm::uvec2& Tile) -> FChunkPosition
{
	FChunkPosition Result;
	Result.Chunk = { Tile.x >> World.ChunkShift, Tile.y >> World.ChunkShift };
	Result.Tile = { Tile.x & World.ChunkMask, Tile.y & World.ChunkMask };
	return Result;
}

auto GetTileValue(const FTileWorld& World, const glm::uvec2& Tile) -> u32
{
	FChunkPosition ChunkPos = GetChunkPosition(World, Tile);
	FTileChunk* Chunk = GetChunk(World, ChunkPos.Chunk);
	if (Chunk)
		return GetTileValue(World, *Chunk, ChunkPos.Tile);
	return 0;
}

inline auto IsWorldPointEmpty(const FTileWorld& World, const FWorldPosition& Position) -> bool
{
	return GetTileValue(World, Position.Tile) == 0;
}

auto DrawRect(FOffscreenBuffer& Buffer, glm::vec2 InMin, glm::vec2 InMax, glm::vec3 InColor) -> void
{
	// TODO(Rafis): assert on (Min.x >= Max.x || Min.y >= Max.y)

	glm::ivec2 Min = glm::round(InMin);
	glm::ivec2 Max = glm::round(InMax);

	Min = glm::clamp(Min, glm::ivec2(0, 0), glm::ivec2(Buffer.Width, Buffer.Height));
	Max = glm::clamp(Max, glm::ivec2(0, 0), glm::ivec2(Buffer.Width, Buffer.Height));

	u32 Pitch = Buffer.Width * Buffer.BytesPerPixel;
	u8* Row = Buffer.Backbuffer + Min.x * Buffer.BytesPerPixel + Min.y * Pitch;

	glm::u8vec3 Color = InColor * 255.f;

	for (u32 Y = Min.y; Y < Max.y; ++Y)
	{
		u32* Pixel = (u32*)Row;
		for (u32 X = Min.x; X < Max.x; ++X)
		{
			*Pixel++ = Color.r << 16 | Color.g << 8 | Color.b << 0;
		}
		Row += Pitch;
	}
}

auto DrawUVGradient(FOffscreenBuffer& Buffer, FGameState& State, FInputGamepad& Gamepad, f32 DeltaTime)
{
	u32 Width = Buffer.Width;
	u32 Height = Buffer.Height;
	u32 Pitch = Width * Buffer.BytesPerPixel;
	u8* Row = Buffer.Backbuffer;

	State.xOffset += Gamepad.GamepadThumbstickLeft.X * State.OffsetSpeed * DeltaTime;
	State.yOffset += Gamepad.GamepadThumbstickLeft.Y * State.OffsetSpeed * DeltaTime;

	for (u32 Y = 0; Y < Height; ++Y)
	{
		u32* Pixel = (u32*)Row;

		for (u32 X = 0; X < Width; ++X)
		{
			u8 C1 = glm::abs(X + State.xOffset);
			u8 C2 = glm::abs(Y + State.yOffset);

			*Pixel++ = ((C2 << 16) | C1);
		}

		Row += Pitch;
	}
}


RE_API auto GameUpdateAndRender(FGameMemory& Memory, f32 DeltaTime, FOffscreenBuffer& Buffer, const FInputGamepad& Gamepad) -> void
{
	FGameState& State = *(FGameState*)Memory.PermanentStorage;

	FTileChunk Chunk;
	Chunk.Tiles = &s_TileMap[0][0];

	FTileWorld World = {};
	World.ChunkShift = 8;
	World.ChunkMask = 0xFF;
	World.Tilemaps = &Chunk;
	World.WorldDimension = { 1, 1 };
	World.TilemapDimension = { s_TileMapX, s_TileMapY };
	World.WorldPosition = glm::vec2(0.5f * Buffer.Width, 0.5f * Buffer.Height);
	World.Padding = glm::vec2(1.f);
	World.TileWidthInMeter = 1.5f;
	World.TileWidthInPixels = 60;
	World.PixelsPerMeter = World.TileWidthInPixels / World.TileWidthInMeter;

	if (Gamepad.GamepadOption.bPressed)
	{
		State.PlayerPosition.Tile = glm::uvec2(5, 1);
		State.PlayerPosition.Local = glm::vec2(0.0f, 0.0f);
	}

	auto LeftStickDirection = glm::vec2(Gamepad.GamepadThumbstickLeft.X, Gamepad.GamepadThumbstickLeft.Y);
	if (Gamepad.GamepadDpadUp.bPressed)
	{
		LeftStickDirection.y = 1.0f;
	}
	if (Gamepad.GamepadDpadDown.bPressed)
	{
		LeftStickDirection.y = -1.0f;
	}
	if (Gamepad.GamepadDpadRight.bPressed)
	{
		LeftStickDirection.x = 1.0f;
	}
	if (Gamepad.GamepadDpadLeft.bPressed)
	{
		LeftStickDirection.x = -1.0f;
	}

	if (LeftStickDirection.x != 0 || LeftStickDirection.y != 0.0)
	{
		LeftStickDirection = glm::normalize(LeftStickDirection);
	}
	auto CurrentSpeedInMeters = (LeftStickDirection * State.PlayerSpeed * DeltaTime);

	auto NewPlayerPos = State.PlayerPosition;
	auto NewPlayerPosLeft = State.PlayerPosition;
	auto NewPlayerPosRight = State.PlayerPosition;

	NewPlayerPos.Local += CurrentSpeedInMeters;
	NewPlayerPosLeft.Local += CurrentSpeedInMeters + State.PlayerSize * glm::vec2(-0.5f, -0.5f);
	NewPlayerPosRight.Local += CurrentSpeedInMeters + State.PlayerSize * glm::vec2(0.5f, -0.5f);

	NewPlayerPos = CalculateWorldPosition(World, NewPlayerPos);
	NewPlayerPosLeft = CalculateWorldPosition(World, NewPlayerPosLeft);
	NewPlayerPosRight = CalculateWorldPosition(World, NewPlayerPosRight);

	if (IsWorldPointEmpty(World, NewPlayerPos) && IsWorldPointEmpty(World, NewPlayerPosLeft) && IsWorldPointEmpty(World, NewPlayerPosRight))
	{
		State.PlayerPosition = NewPlayerPos;
	}

	// Clear screen
	DrawRect(Buffer, { 0.0f, 0.0f }, { Buffer.Width, Buffer.Height }, { 0.08f, 0.17f, 0.85f });

	i32 HalfTilesPerScreenHorizontal = (i32)(glm::floor(Buffer.Width / World.TileWidthInPixels) + 1) / 2;
	i32 HalfTilesPerScreenVertical = (i32)(glm::floor(Buffer.Height / World.TileWidthInPixels) + 1) / 2;

	// Draw tiles
	for (i32 RelRow = -HalfTilesPerScreenVertical; RelRow <= HalfTilesPerScreenVertical; ++RelRow)
	{
		for (i32 RelColumn = -HalfTilesPerScreenHorizontal; RelColumn <= HalfTilesPerScreenHorizontal; ++RelColumn)
		{
			auto Tile = glm::uvec2(glm::ivec2(State.PlayerPosition.Tile) + glm::ivec2{ RelColumn, RelRow });

			auto TileID = GetTileValue(World, Tile);
			auto DrawSize = glm::vec2(World.TileWidthInPixels) - World.Padding * 2.0f;
			auto DrawPosition = World.WorldPosition;
			// Account for the 0,0 position of the tile is at the center now
			DrawPosition.x -= World.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
			DrawPosition.y += World.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
			// Account for the player position as everything is drawn relative to the player
			DrawPosition -= (State.PlayerPosition.Local * World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
			DrawPosition += (glm::vec2(RelColumn, RelRow) * World.TileWidthInPixels + World.Padding) * glm::vec2(1.0f, -1.0f);
			DrawPosition += DrawSize * glm::vec2(0.0f, -1.0f);
			DrawRect(Buffer, DrawPosition, DrawPosition + DrawSize, { TileID, TileID, TileID });

#if defined(RE_INTERNAL)
			// Debug draws
			{
				// DrawRect(Buffer, DrawPosition, DrawPosition + 1.0f, { 1, 0, 0 });
				// DrawRect(Buffer, DrawPosition - World.Padding * 2.0f, DrawPosition, { 1, 1, 0 });
			}
#endif
		}
	}

	// Draw Player
	{
		auto DrawSize = State.PlayerSize * World.PixelsPerMeter;
		auto DrawPosition = World.WorldPosition;
		// DrawPosition += (Position.Local * World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
		DrawPosition -= DrawSize * glm::vec2(0.5f, 0.5f);

#if defined(RE_INTERNAL)
		// Draw the tile player is on
		{
			auto DebugDrawSize = glm::vec2(World.TileWidthInPixels);
			auto DebugDrawPosition = World.WorldPosition;
			// Account for the 0,0 position of the tile is at the center now
			DebugDrawPosition.x -= World.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
			DebugDrawPosition.y += World.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
			// Account for the player position as everything is drawn relative to the player
			DebugDrawPosition -= (State.PlayerPosition.Local * World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
			DebugDrawPosition += glm::vec2(0, 0) * World.TileWidthInPixels * glm::vec2(1.0f, -1.0f);
			DebugDrawPosition += DebugDrawSize * glm::vec2(0.0f, -1.0f);
			DrawRect(Buffer, DebugDrawPosition, DebugDrawPosition + DebugDrawSize, glm::vec3{ 0.3f, 0.8f, 0.1f });
		}
#endif

		DrawRect(Buffer, DrawPosition, DrawPosition + DrawSize, glm::vec3(0.65f, 0.08f, 0.15f));
	}

	// Center of the screen
	{
		DrawRect(Buffer, World.WorldPosition - glm::vec2(4.f), World.WorldPosition + glm::vec2(4.f), glm::vec3(0.15f, 0.08f, 0.10f));
	}
}
