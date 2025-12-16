#include <Platform.h>

#include "GameMath.h"

struct FWorldPosition
{
#if 1
	glm::ivec2 Tilemap;
	glm::ivec2 Tile;
#else
	glm::uvec2 _Tile;
#endif
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

struct FTimeMap
{
	u32* Tiles;
};

struct FTileWorld
{
	glm::ivec2 WorldDimension;

	glm::ivec2 TilemapDimension;
	FTimeMap* Tilemaps;

	glm::vec2 WorldPosition; // Upper left corner of the tile map
	glm::vec2 Padding;		 // Padding for drawing

	f32 TileWidthInMeter;
	f32 TileWidthInPixels;
	f32 PixelsPerMeter;
};

constexpr u32 s_TileMapX = 31;
constexpr u32 s_TileMapY = 19;

static u32 s_TileMap0[s_TileMapY][s_TileMapX] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1 },
	{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 }, //
	{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

static u32 s_TileMap1[s_TileMapY][s_TileMapX] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, //
	{ 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1 },
	{ 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

static u32 s_TileMap2[s_TileMapY][s_TileMapX] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1 },
	{ 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0 }, //
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
	{ 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
	{ 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

static u32 s_TileMap3[s_TileMapY][s_TileMapX] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
	{ 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1 },
	{ 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 },
	{ 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1 },
	{ 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 }, //
	{ 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1 },
	{ 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1 },
	{ 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1 },
	{ 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
	{ 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

RE_API auto GameGetSurfaceDesc() -> FGameSurfaceDesc*
{
	static FGameSurfaceDesc Props;
	Props.Width = s_TileMapX * 60.f;
	Props.Height = s_TileMapY * 60.f;
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

	State.PlayerPosition.Tilemap = glm::ivec2(0);
	State.PlayerPosition.Tile = glm::ivec2(5, 1);
	State.PlayerPosition.Local = glm::vec2(0.75f, 1.25f);
}

inline auto GetTimemapValueUnchecked(const FTileWorld& World, const FTimeMap& TileMap, glm::ivec2 Tile) -> u32
{
	return TileMap.Tiles[Tile.y * World.TilemapDimension.x + Tile.x];
}

inline auto IsTilemapPointEmpty(const FTileWorld& World, const FTimeMap& TileMap, glm::ivec2 TestTile) -> bool
{
	return GetTimemapValueUnchecked(World, TileMap, TestTile) == 0;
}

inline auto GetTimemap(const FTileWorld& World, glm::ivec2 Map) -> FTimeMap*
{
	if (Map >= glm::ivec2(0) && Map < World.WorldDimension)
		return &World.Tilemaps[Map.y * World.WorldDimension.x + Map.x];

	return nullptr;
}

inline auto CalculateWorldPosition(const FTileWorld& World, const FWorldPosition& Position) -> FWorldPosition
{
	glm::vec2 PosInMeters = (glm::vec2)Position.Tile * World.TileWidthInMeter + Position.Local;

	FWorldPosition NewPosition = Position;
	NewPosition.Tile = glm::floor(PosInMeters / World.TileWidthInMeter);
	NewPosition.Local = PosInMeters - (glm::vec2)NewPosition.Tile * World.TileWidthInMeter;
	NewPosition.Tilemap = Position.Tilemap;

	if (NewPosition.Tile.x < 0)
	{
		NewPosition.Tile.x += World.TilemapDimension.x;
		--NewPosition.Tilemap.x;
	}
	if (NewPosition.Tile.y < 0)
	{
		NewPosition.Tile.y += World.TilemapDimension.y;
		--NewPosition.Tilemap.y;
	}
	if (NewPosition.Tile.x >= World.TilemapDimension.x)
	{
		NewPosition.Tile.x -= World.TilemapDimension.x;
		++NewPosition.Tilemap.x;
	}
	if (NewPosition.Tile.y >= World.TilemapDimension.y)
	{
		NewPosition.Tile.y -= World.TilemapDimension.y;
		++NewPosition.Tilemap.y;
	}

	return NewPosition;
}

auto IsWorldPointEmpty(const FTileWorld& World, const FWorldPosition& Position) -> bool
{
	FTimeMap* TileMap = GetTimemap(World, Position.Tilemap);
	return TileMap && IsTilemapPointEmpty(World, *TileMap, Position.Tile);
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

auto DrawTilemap(FOffscreenBuffer& Buffer, const FTileWorld& World, const glm::ivec2& TilemapCoords)
{
	auto Tilemap = GetTimemap(World, TilemapCoords);
	if (Tilemap)
	{
		for (u32 Row = 0; Row < World.TilemapDimension.y; ++Row)
		{
			for (auto Column = 0; Column < World.TilemapDimension.x; ++Column)
			{
				auto TileID = GetTimemapValueUnchecked(World, *Tilemap, glm::ivec2{ Column, Row });
				auto DrawSize = glm::vec2(World.TileWidthInPixels) - World.Padding * 2.0f;
				auto DrawPosition = World.WorldPosition;
				DrawPosition += (glm::vec2(Column, Row) * World.TileWidthInPixels + World.Padding) * glm::vec2(1.0f, -1.0f);
				DrawPosition += DrawSize * glm::vec2(0.0f, -1.0f);
				DrawRect(Buffer, DrawPosition, DrawPosition + DrawSize, { TileID, TileID, TileID });
				// DrawRect(Buffer, DrawPosition, DrawPosition + 1.0f, { 1, 0, 0 });
				// DrawRect(Buffer, DrawPosition - World.Padding * 2.0f, DrawPosition, { 1, 1, 0 });
			}
		}
	}
}

auto DrawPlayer(FOffscreenBuffer& Buffer, const FTileWorld& World, const FWorldPosition& Position, const glm::vec2& Size, const glm::vec3& Color)
{
	auto DrawSize = Size * World.PixelsPerMeter;
	auto DrawPosition = World.WorldPosition;
	DrawPosition += (glm::vec2(Position.Tile) * World.TileWidthInPixels + Position.Local * World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
	DrawPosition -= DrawSize * glm::vec2(0.5f, 1.0f);

#if defined(RE_INTERNAL)
	// Draw the tile player is on
	{
		auto DebugDrawSize = glm::vec2(World.TileWidthInPixels);
		auto DebugDrawPosition = World.WorldPosition;
		DebugDrawPosition += glm::vec2(Position.Tile) * World.TileWidthInPixels * glm::vec2(1.0f, -1.0f);
		DebugDrawPosition += DebugDrawSize * glm::vec2(0.0f, -1.0f);
		DrawRect(Buffer, DebugDrawPosition, DebugDrawPosition + DebugDrawSize, glm::vec3{ 0.3f, 0.8f, 0.1f });
	}
#endif

	DrawRect(Buffer, DrawPosition, DrawPosition + DrawSize, Color);
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

auto ClearScreen(FOffscreenBuffer& Buffer, glm::vec3 InColor)
{
	DrawRect(Buffer, { 0.0f, 0.0f }, { Buffer.Width, Buffer.Height }, InColor);
}

RE_API auto GameUpdateAndRender(FGameMemory& Memory, f32 DeltaTime, FOffscreenBuffer& Buffer, const FInputGamepad& Gamepad) -> void
{
	FGameState& State = *(FGameState*)Memory.PermanentStorage;

	FTimeMap TileMaps[2][2] = { { TileMaps[0][0].Tiles = &s_TileMap0[0][0], TileMaps[0][1].Tiles = &s_TileMap1[0][0] },
		{ TileMaps[1][0].Tiles = &s_TileMap2[0][0], TileMaps[1][1].Tiles = &s_TileMap3[0][0] } };

	FTileWorld World = {};
	World.Tilemaps = &TileMaps[0][0];
	World.WorldDimension = { 2, 2 };
	World.TilemapDimension = { s_TileMapX, s_TileMapY };
	World.WorldPosition = glm::vec2(0.0f, Buffer.Height);
	World.Padding = glm::vec2(1.f);
	World.TileWidthInMeter = 1.5f;
	World.TileWidthInPixels = 60;
	World.PixelsPerMeter = World.TileWidthInPixels / World.TileWidthInMeter;

	if (Gamepad.GamepadX.bPressed)
	{
		State.PlayerPosition.Tilemap = { 0, 1 };
	}
	if (Gamepad.GamepadY.bPressed)
	{
		State.PlayerPosition.Tilemap = { 1, 1 };
	}
	if (Gamepad.GamepadA.bPressed)
	{
		State.PlayerPosition.Tilemap = { 0, 0 };
	}
	if (Gamepad.GamepadB.bPressed)
	{
		State.PlayerPosition.Tilemap = { 1, 0 };
	}

	if (Gamepad.GamepadOption.bPressed)
	{
		State.PlayerPosition.Tilemap = glm::ivec2(0);
		State.PlayerPosition.Tile = glm::ivec2(5, 1);
		State.PlayerPosition.Local = glm::vec2(0.75f, 1.25f);
	}

	auto LeftStickDirection = glm::vec2(Gamepad.GamepadThumbstickLeft.X, Gamepad.GamepadThumbstickLeft.Y);
	if (Gamepad.GamepadThumbstickLeft.X != 0 || Gamepad.GamepadThumbstickLeft.Y != 0.0)
	{
		LeftStickDirection = glm::normalize(LeftStickDirection);
	}
	auto CurrentSpeedInMeters = (LeftStickDirection * State.PlayerSpeed * DeltaTime);
	auto PartialPlayerSizeInMeters = State.PlayerSize * glm::vec2(0.5f, 0.0f);

	auto NewPlayerPos = State.PlayerPosition;
	auto NewPlayerPosLeft = State.PlayerPosition;
	auto NewPlayerPosRight = State.PlayerPosition;

	NewPlayerPos.Local += CurrentSpeedInMeters;
	NewPlayerPosLeft.Local += CurrentSpeedInMeters - PartialPlayerSizeInMeters;
	NewPlayerPosRight.Local += CurrentSpeedInMeters + PartialPlayerSizeInMeters;

	NewPlayerPos = CalculateWorldPosition(World, NewPlayerPos);
	NewPlayerPosLeft = CalculateWorldPosition(World, NewPlayerPosLeft);
	NewPlayerPosRight = CalculateWorldPosition(World, NewPlayerPosRight);

	if (IsWorldPointEmpty(World, NewPlayerPos) && IsWorldPointEmpty(World, NewPlayerPosLeft) && IsWorldPointEmpty(World, NewPlayerPosRight))
	{
		State.PlayerPosition = NewPlayerPos;
	}

	ClearScreen(Buffer, { 0.08f, 0.17f, 0.85f });

	DrawTilemap(Buffer, World, State.PlayerPosition.Tilemap);

	DrawPlayer(Buffer, World, State.PlayerPosition, State.PlayerSize, glm::vec3(0.65f, 0.08f, 0.15f));
}
