#include <Platform.h>

#include "GameMath.h"
#include "Memory.h"
#include "Tilemap.cpp"

using namespace re;

struct FWorld
{
	Tilemap::FWorld* Tilemap;
};

struct FGameState
{
	f32 xOffset;
	f32 yOffset;
	f32 OffsetSpeed;

	Memory::Arena::FMemoryArena WorldArena;
	FWorld* World;

	glm::vec2 DrawPosition; // Upper left corner of the tile map
	glm::vec2 Padding;		// Padding for drawing

	Tilemap::FTilemapPosition PlayerPosition;
	glm::vec2 PlayerSize;
	glm::vec2 PlayerSpeed;
};

static FGameSurfaceDesc Props;

RE_API auto GameGetSurfaceDesc() -> FGameSurfaceDesc*
{
	Props.Width = 1200.f;
	Props.Height = 840.f;
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

	usize WorldArenaSize = Memory.PermanentStorageSize - sizeof(FGameState);
	void* WorldArenaPointer = (u8*)Memory.PermanentStorage + sizeof(FGameState);
	Memory::Arena::Initialize(State.WorldArena, WorldArenaSize, WorldArenaPointer);

	State.World = Memory::Arena::Allocate<FWorld>(State.WorldArena);
	State.World->Tilemap = Memory::Arena::Allocate<Tilemap::FWorld>(State.WorldArena);
	auto& Tilemap = *State.World->Tilemap;

	Tilemap.ChunkShift = 8;
	Tilemap.ChunkMask = 0xFF;
	Tilemap.WorldDimension = { 4, 4 };
	Tilemap.ChunkDimension = { 16, 16 };
	Tilemap.TileWidthInMeter = 1.5f;
	Tilemap.TileWidthInPixels = 60;
	Tilemap.PixelsPerMeter = Tilemap.TileWidthInPixels / Tilemap.TileWidthInMeter;
	Tilemap.Chunks = Memory::Arena::AllocateArray<Tilemap::FChunk>(State.WorldArena, Tilemap.WorldDimension.x * Tilemap.WorldDimension.y);

	for (u32 ScreenY = 0; ScreenY < Tilemap.WorldDimension.y; ++ScreenY)
	{
		for (u32 ScreenX = 0; ScreenX < Tilemap.WorldDimension.x; ++ScreenX)
		{
			Tilemap::GetChunk(Tilemap, { ScreenX, ScreenY })->Tiles =
				Memory::Arena::AllocateArray<u32>(State.WorldArena, Tilemap.ChunkDimension.x * Tilemap.ChunkDimension.y);

			for (u32 TileY = 0; TileY < Tilemap.ChunkDimension.y; ++TileY)
			{
				for (u32 TileX = 0; TileX < Tilemap.ChunkDimension.x; ++TileX)
				{
					auto TilePosition = Tilemap::FTilePosition(ScreenX * Tilemap.ChunkDimension.x + TileX, ScreenY * Tilemap.ChunkDimension.y + TileY);
					float r = rand();
					float r2 = r / INT32_MAX;
					u32 Value = r2 > 0.2f ? 0 : 1;
					Tilemap::SetTileValue(State.WorldArena, Tilemap, TilePosition, Value);
				}
			}
		}
	}
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

	State.DrawPosition = glm::vec2(0.5f * Buffer.Width, 0.5f * Buffer.Height);
	State.Padding = glm::vec2(1.f);

	auto& Tilemap = *State.World->Tilemap;

	if (Gamepad.GamepadOption.bPressed)
	{
		State.PlayerPosition.Tile = glm::uvec2(15, 15);
		State.PlayerPosition.Local = glm::vec2(0.0f, 0.74f);
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

	NewPlayerPos = Tilemap::RecalculateTilemapPosition(Tilemap, NewPlayerPos);
	NewPlayerPosLeft = Tilemap::RecalculateTilemapPosition(Tilemap, NewPlayerPosLeft);
	NewPlayerPosRight = Tilemap::RecalculateTilemapPosition(Tilemap, NewPlayerPosRight);

	if (Tilemap::IsTilemapPointEmpty(Tilemap, NewPlayerPos) && Tilemap::IsTilemapPointEmpty(Tilemap, NewPlayerPosLeft)
		&& Tilemap::IsTilemapPointEmpty(Tilemap, NewPlayerPosRight))
	{
		State.PlayerPosition = NewPlayerPos;
	}

	// Clear screen
	DrawRect(Buffer, { 0.0f, 0.0f }, { Buffer.Width, Buffer.Height }, { 0.08f, 0.17f, 0.85f });

	i32 HalfTilesPerScreenHorizontal = (i32)(glm::floor(Buffer.Width / Tilemap.TileWidthInPixels) + 1) / 2;
	i32 HalfTilesPerScreenVertical = (i32)(glm::floor(Buffer.Height / Tilemap.TileWidthInPixels) + 1) / 2;

	// Draw tiles
	for (i32 RelRow = -HalfTilesPerScreenVertical; RelRow <= HalfTilesPerScreenVertical; ++RelRow)
	{
		for (i32 RelColumn = -HalfTilesPerScreenHorizontal; RelColumn <= HalfTilesPerScreenHorizontal; ++RelColumn)
		{
			auto Tile = Tilemap::FTilePosition(glm::ivec2(State.PlayerPosition.Tile) + glm::ivec2{ RelColumn, RelRow });

			auto TileID = Tilemap::GetTileValue(Tilemap, Tile);
			auto DrawSize = glm::vec2(Tilemap.TileWidthInPixels) - State.Padding * 2.0f;
			auto DrawPosition = State.DrawPosition;
			// Account for the 0,0 position of the tile is at the center now
			DrawPosition.x -= Tilemap.TileWidthInMeter * Tilemap.PixelsPerMeter * 0.5f;
			DrawPosition.y += Tilemap.TileWidthInMeter * Tilemap.PixelsPerMeter * 0.5f;
			// Account for the player position as everything is drawn relative to the player
			DrawPosition -= (State.PlayerPosition.Local * Tilemap.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
			DrawPosition += (glm::vec2(RelColumn, RelRow) * Tilemap.TileWidthInPixels + State.Padding) * glm::vec2(1.0f, -1.0f);
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
		auto DrawSize = State.PlayerSize * Tilemap.PixelsPerMeter;
		auto DrawPosition = State.DrawPosition;
		// DrawPosition += (Position.Local * State.World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
		DrawPosition -= DrawSize * glm::vec2(0.5f, 0.5f);

#if defined(RE_INTERNAL)
		// Draw the tile player is on
		{
			auto DebugDrawSize = glm::vec2(Tilemap.TileWidthInPixels);
			auto DebugDrawPosition = State.DrawPosition;
			// Account for the 0,0 position of the tile is at the center now
			DebugDrawPosition.x -= Tilemap.TileWidthInMeter * Tilemap.PixelsPerMeter * 0.5f;
			DebugDrawPosition.y += Tilemap.TileWidthInMeter * Tilemap.PixelsPerMeter * 0.5f;
			// Account for the player position as everything is drawn relative to the player
			DebugDrawPosition -= (State.PlayerPosition.Local * Tilemap.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
			DebugDrawPosition += glm::vec2(0, 0) * Tilemap.TileWidthInPixels * glm::vec2(1.0f, -1.0f);
			DebugDrawPosition += DebugDrawSize * glm::vec2(0.0f, -1.0f);
			DrawRect(Buffer, DebugDrawPosition, DebugDrawPosition + DebugDrawSize, glm::vec3{ 0.3f, 0.8f, 0.1f });
		}
#endif

		DrawRect(Buffer, DrawPosition, DrawPosition + DrawSize, glm::vec3(0.65f, 0.08f, 0.15f));
	}

	// Center of the screen
	{
		DrawRect(Buffer, State.DrawPosition - glm::vec2(4.f), State.DrawPosition + glm::vec2(4.f), glm::vec3(0.15f, 0.08f, 0.10f));
	}
}
