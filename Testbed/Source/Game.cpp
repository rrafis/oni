#include <Platform.h>

#include "GameMath.h"
#include "Memory.h"
#include "Tilemap.cpp"

#include <filesystem>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace re;

struct FWorld
{
	Tilemap::FWorld* Tilemap;
	glm::vec2 DrawPosition; // Upper left corner of the tile map
	glm::vec2 Padding;		// Padding for drawing
	f32 TileWidthInPixels;
	f32 PixelsPerMeter;
};

struct FImage
{
	u32 Width, Height;
	u8 Channels;
	u32* Pixels;
};

struct FGameState
{
	f32 xOffset;
	f32 yOffset;
	f32 OffsetSpeed;

	Memory::Arena::FMemoryArena WorldArena;
	FWorld* World;

	Tilemap::FTilemapPosition PlayerPosition;
	glm::vec2 PlayerSize;
	glm::vec2 PlayerSpeed;

	FImage Background;
	FImage Player;
};

static FGameSurfaceDesc Props;

constexpr auto ContentPath = "Content";

inline u8 SRGBToUnorm8(u8 c)
{
	float cs = c / 255.0f;
	if (cs <= 0.04045f)
		cs = cs / 12.92f;
	else
		cs = std::pow((cs + 0.055f) / 1.055f, 2.4f);
	cs = std::fmax(0.0f, std::fmin(1.0f, cs));
	return static_cast<u8>(std::round(cs * 255.0f));
}

auto LoadImage(FImage& Image, const char* path) -> i32
{
	auto LoadPath = std::filesystem::path(ContentPath) / path;

	stbi_set_flip_vertically_on_load(false);

	int width, height, channels;
	unsigned char* data = stbi_load(LoadPath.c_str(), &width, &height, &channels, 4);
	channels = 4;

	if (data == nullptr)
	{
		std::cerr << "Failed to load image!" << std::endl;
		return -1;
	}

	auto size = width * height; // * channels;

	Image.Width = width;
	Image.Height = height;
	Image.Channels = channels;
	u32* Pixel = Image.Pixels;
	for (u32 index = 0; index < size * channels; index += 4)
	{
		u8 r = SRGBToUnorm8(data[index + 0]);
		u8 g = SRGBToUnorm8(data[index + 1]);
		u8 b = SRGBToUnorm8(data[index + 2]);
		u8 a = SRGBToUnorm8(data[index + 3]);
		*Pixel++ = a << 24 | r << 16 | g << 8 | b << 0;
	}

	std::cout << "Image loaded: " << width << "x" << height << " with " << channels << " channels." << std::endl;

	// Free memory when done
	stbi_image_free(data);

	return 0;
}

RE_API auto GameGetSurfaceDesc() -> FGameSurfaceDesc*
{
	Props.Width = 1260.f;
	Props.Height = 900.f;
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

	State.PlayerSpeed = glm::vec2{ 36.0f };
	State.PlayerSize = glm::vec2{ 1.0f };

	State.PlayerPosition.Tile = glm::uvec3(10, 7, 0);
	State.PlayerPosition.Local = glm::vec2(0.0f, 0.0f);

	usize WorldArenaSize = Memory.PermanentStorageSize - sizeof(FGameState);
	void* WorldArenaPointer = (u8*)Memory.PermanentStorage + sizeof(FGameState);
	Memory::Arena::Initialize(State.WorldArena, WorldArenaSize, WorldArenaPointer);

	State.World = Memory::Arena::Allocate<FWorld>(State.WorldArena);
	auto& World = *State.World;
	World.Tilemap = Memory::Arena::Allocate<Tilemap::FWorld>(State.WorldArena);
	auto& Tilemap = *World.Tilemap;

	Tilemap.WorldDimension = { 16 * 16, 16 * 16, 2 };
	Tilemap.ChunkDimension = { 16, 16 };
	Tilemap.TileWidthInMeter = 1.5f;
	Tilemap.Chunks =
		Memory::Arena::AllocateArray<Tilemap::FChunk>(State.WorldArena, Tilemap.WorldDimension.x * Tilemap.WorldDimension.y * Tilemap.WorldDimension.z);

	World.TileWidthInPixels = 60;
	World.PixelsPerMeter = World.TileWidthInPixels / Tilemap.TileWidthInMeter;

	auto ScreenChunkDim = glm::uvec2(10, 10);
	auto ScreenTileDim = glm::uvec2(21, 15);
	auto ScreenTileHalfDim = ScreenTileDim / glm::uvec2(2);
	u32 ScreenZ = 0;
	auto Screen = glm::uvec2(0);
	for (u32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex)
	{
		u32 RandomValue = rand() % 3;
		for (u32 TileY = 0; TileY < ScreenTileDim.y; ++TileY)
		{
			for (u32 TileX = 0; TileX < ScreenTileDim.x; ++TileX)
			{
				Tilemap::ETileType TileValue = Tilemap::ETileType::Empty;
				if (TileY == 0 || TileY == ScreenTileDim.y - 1)
				{
					TileValue = Tilemap::ETileType::Wall;
					if (TileX >= ScreenTileHalfDim.x - 1 && TileX <= ScreenTileHalfDim.x + 1)
					{
						TileValue = Tilemap::ETileType::Empty;
					}
				}

				if (TileX == 0 || TileX == ScreenTileDim.x - 1)
				{
					TileValue = Tilemap::ETileType::Wall;
					if (TileY >= ScreenTileHalfDim.y - 1 && TileY <= ScreenTileHalfDim.y + 1)
					{
						TileValue = Tilemap::ETileType::Empty;
					}
				}

				if (RandomValue == 2)
				{
					if (TileX == 10 && TileY == 10)
					{
						TileValue = ScreenZ == 0 ? Tilemap::ETileType::StairDown : Tilemap::ETileType::StairUp;
					}
				}

				auto TilePosition = Tilemap::FTilePosition(Screen * ScreenTileDim + glm::uvec2(TileX, TileY), ScreenZ);
				Tilemap::SetTileValue(State.WorldArena, Tilemap, TilePosition, TileValue);
			}
		}
		if (RandomValue == 2)
		{
			ScreenZ = (ScreenZ + 1) % Tilemap.WorldDimension.z;
		}
		else if (RandomValue == 0)
		{
			Screen.x += 1;
		}
		else
		{
			Screen.y += 1;
		}
	}

	State.Background.Pixels = (u32*)Memory.TempStorage;
	LoadImage(State.Background, "test/background.png");
	State.Player.Pixels = (u32*)((u8*)Memory.TempStorage + State.Background.Width * State.Background.Height * State.Background.Channels);
	LoadImage(State.Player, "test/link.png");
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

auto DrawImage(FOffscreenBuffer& Buffer, glm::vec2 InMin, glm::vec2 InMax, const FImage& Image) -> void
{
	// TODO(Rafis): assert on (Min.x >= Max.x || Min.y >= Max.y)

	glm::ivec2 Min = glm::round(InMin);
	glm::ivec2 Max = glm::round(InMax);

	Min = glm::clamp(Min, glm::ivec2(0), glm::ivec2(Buffer.Width, Buffer.Height));
	Max = glm::clamp(Max, glm::ivec2(0), glm::ivec2(Buffer.Width, Buffer.Height));

	u32 Pitch = Buffer.Width * Buffer.BytesPerPixel;
	u8* Row = Buffer.Backbuffer + Min.x * Buffer.BytesPerPixel + Min.y * Pitch;

	for (u32 Y = 0; Y < Image.Height && Y < Buffer.Height; ++Y)
	{
		u32* Pixel = (u32*)Row;
		for (u32 X = 0; X < Image.Width && X < Buffer.Width; ++X)
		{
			u32 Color = Image.Pixels[Y * Image.Width + X];
			u8 alpha = Color >> 24 & 0xFF;
			if (alpha != 0)
			{
				if (alpha == 255)
				{
					*Pixel = Color;
				}
				else
				{
					auto DestColor = glm::vec3(Color >> 16 & 0xFF, Color >> 8 & 0xFF, Color >> 0 & 0xFF);
					auto SrcColor = glm::vec3(*Pixel >> 16 & 0xFF, *Pixel >> 8 & 0xFF, *Pixel >> 0 & 0xFF);
					auto Result = glm::u8vec3(glm::mix(SrcColor, DestColor, alpha / 255.f));
					*Pixel = Result.r << 16 | Result.g << 8 | Result.b << 0;
				}
			}
			Pixel++;
		}
		Row += Pitch;
	}
}

RE_API auto GameUpdateAndRender(FGameMemory& Memory, f32 DeltaTime, FOffscreenBuffer& Buffer, const FInputGamepad& Gamepad) -> void
{
	FGameState& State = *(FGameState*)Memory.PermanentStorage;
	auto& World = *State.World;
	auto& Tilemap = *World.Tilemap;

	World.DrawPosition = glm::vec2(0.5f * Buffer.Width, 0.5f * Buffer.Height);
	World.Padding = glm::vec2(1.f);

	static bool bOptionPressed = false;
	static bool bOptionWasPressed = false;
	bOptionWasPressed = bOptionPressed && Gamepad.GamepadOption.bPressed;
	bOptionPressed = Gamepad.GamepadOption.bPressed;
	if (bOptionPressed && !bOptionWasPressed)
	{
		State.PlayerPosition.Tile = glm::uvec3(20, 15, State.PlayerPosition.Tile.z);
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

	NewPlayerPos = Tilemap::RecalculateTilemapPosition(Tilemap, NewPlayerPos);
	NewPlayerPosLeft = Tilemap::RecalculateTilemapPosition(Tilemap, NewPlayerPosLeft);
	NewPlayerPosRight = Tilemap::RecalculateTilemapPosition(Tilemap, NewPlayerPosRight);

	if (Tilemap::IsTilemapPointEmpty(Tilemap, NewPlayerPos) && Tilemap::IsTilemapPointEmpty(Tilemap, NewPlayerPosLeft)
		&& Tilemap::IsTilemapPointEmpty(Tilemap, NewPlayerPosRight))
	{
		if (!Tilemap::OnSameTile(Tilemap, State.PlayerPosition, NewPlayerPos))
		{
			auto TileID = Tilemap::GetTileValue(Tilemap, NewPlayerPos.Tile);
			if (TileID == Tilemap::ETileType::StairDown)
			{
				NewPlayerPos.Tile.z = 1;
			}
			else if (TileID == Tilemap::ETileType::StairUp)
			{
				NewPlayerPos.Tile.z = 0;
			}
		}
		State.PlayerPosition = NewPlayerPos;
	}

	// Clear screen
	DrawRect(Buffer, glm::vec2{ 0.0f, 0.0f }, glm::vec2{ Buffer.Width, Buffer.Height }, glm::vec3{ 0.08f, 0.17f, 0.85f });

	DrawImage(Buffer, glm::vec2{ 0, 0 }, glm::vec2{ State.Background.Width, State.Background.Height }, State.Background);

	i32 HalfTilesPerScreenHorizontal = (i32)(glm::floor(Buffer.Width / World.TileWidthInPixels) + 1) / 2;
	i32 HalfTilesPerScreenVertical = (i32)(glm::floor(Buffer.Height / World.TileWidthInPixels) + 1) / 2;

	// Draw tiles
	for (i32 RelRow = -HalfTilesPerScreenVertical; RelRow <= HalfTilesPerScreenVertical; ++RelRow)
	{
		for (i32 RelColumn = -HalfTilesPerScreenHorizontal; RelColumn <= HalfTilesPerScreenHorizontal; ++RelColumn)
		{
			auto Tile = Tilemap::FTilePosition(glm::ivec2(State.PlayerPosition.Tile) + glm::ivec2{ RelColumn, RelRow }, State.PlayerPosition.Tile.z);
			auto TileID = Tilemap::GetTileValue(Tilemap, Tile);

			if (TileID != Tilemap::ETileType::Invalid)
			{
				auto DrawSize = glm::vec2(World.TileWidthInPixels) - World.Padding * 2.0f;
				auto DrawPosition = World.DrawPosition;
				// Account for the 0,0 position of the tile is at the center now
				DrawPosition.x -= Tilemap.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
				DrawPosition.y += Tilemap.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
				// Account for the player position as everything is drawn relative to the player
				DrawPosition -= (State.PlayerPosition.Local * World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
				DrawPosition += (glm::vec2(RelColumn, RelRow) * World.TileWidthInPixels + World.Padding) * glm::vec2(1.0f, -1.0f);
				DrawPosition += DrawSize * glm::vec2(0.0f, -1.0f);

				auto Color = glm::vec3(0);
				switch (TileID)
				{
					case Tilemap::ETileType::Wall:
					{
						Color = glm::vec3(1);
						break;
					}
					case Tilemap::ETileType::Uninitialized:
					{
						Color = glm::vec3(0.5f);
						break;
					}
					case Tilemap::ETileType::StairUp:
					{
						Color = glm::vec3(1, 1, 0);
						break;
					}
					case Tilemap::ETileType::StairDown:
					{
						Color = glm::vec3(0, 1, 1);
						break;
					}
					default:
					{
						Color = glm::vec3(0.f);
						break;
					}
				}
				DrawRect(Buffer, DrawPosition, DrawPosition + DrawSize, Color);

#if defined(RE_INTERNAL)
				// Debug draws
				{
					// DrawRect(Buffer, DrawPosition, DrawPosition + 1.0f, { 1, 0, 0 });
					// DrawRect(Buffer, DrawPosition - World.Padding * 2.0f, DrawPosition, { 1, 1, 0 });
				}
#endif
			}
		}
	}

	// Draw Player
	{
		auto DrawSize = State.PlayerSize * World.PixelsPerMeter;
		auto DrawPosition = World.DrawPosition;
		// DrawPosition += (Position.Local * State.World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
		DrawPosition -= DrawSize * glm::vec2(0.5f, 0.5f);

#if defined(RE_INTERNAL)
		// Draw the tile player is on
		{
			auto DebugDrawSize = glm::vec2(World.TileWidthInPixels);
			auto DebugDrawPosition = World.DrawPosition;
			// Account for the 0,0 position of the tile is at the center now
			DebugDrawPosition.x -= Tilemap.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
			DebugDrawPosition.y += Tilemap.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
			// Account for the player position as everything is drawn relative to the player
			DebugDrawPosition -= (State.PlayerPosition.Local * World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
			DebugDrawPosition += glm::vec2(0, 0) * World.TileWidthInPixels * glm::vec2(1.0f, -1.0f);
			DebugDrawPosition += DebugDrawSize * glm::vec2(0.0f, -1.0f);
			DrawRect(Buffer, DebugDrawPosition, DebugDrawPosition + DebugDrawSize, glm::vec3{ 0.3f, 0.8f, 0.1f });
		}
#endif

		DrawRect(Buffer, DrawPosition, DrawPosition + DrawSize, glm::vec3(0.65f, 0.08f, 0.15f));

		DrawImage(Buffer, DrawPosition, DrawPosition + DrawSize, State.Player);
	}

	// Center of the screen
	{
		DrawRect(Buffer, World.DrawPosition - glm::vec2(4.f), World.DrawPosition + glm::vec2(4.f), glm::vec3(0.15f, 0.08f, 0.10f));
	}
}
