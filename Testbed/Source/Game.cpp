#include <Platform.h>

#include "GameMath.h"
#include "Memory.h"
#include "Tilemap.cpp"

#include <filesystem>
#include <iostream>

#include <entt/entt.hpp>

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

	glm::uvec2 ScreenTileDim;
	glm::uvec2 ScreenTileDimHalf;
};

struct FImage
{
	u32 Width, Height;
	u8 Channels;
	u32* Pixels;
};

struct FSprite
{
	FImage* Texture;
	glm::ivec2 Pivot;
	glm::uvec2 Min;
	glm::uvec2 Max;
};

struct FPlayerState
{
	Tilemap::FTilemapPosition Position;
	glm::vec2 Velocity;
};

struct FGameState
{
	f32 xOffset;
	f32 yOffset;
	f32 OffsetSpeed;

	Memory::Arena::FMemoryArena WorldArena;
	FWorld* World;

	entt::handle CameraTarget;
	Tilemap::FTilemapPosition CameraPosition;

	glm::vec2 PlayerSize;
	f32 PlayerAcceleration;
	f32 PlayerFrictionCoeficient;
	f32 PlayerSpriteIdleIndex;
	f32 PlayerSpriteMoveIndex;

	FImage BackgroundTexture;
	FImage PlayerTexture;

	FSprite BgSprite;
	std::array<FSprite, 4> PlayerSpriteIdle;

	std::array<FSprite, 6> PlayerSpriteMoveDown;
	std::array<FSprite, 6> PlayerSpriteMoveRight;
	std::array<FSprite, 6> PlayerSpriteMoveLeft;
	std::array<FSprite, 6> PlayerSpriteMoveUp;

	entt::registry Registry;
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

auto AddPlayer(FGameState& State) -> void
{
	entt::handle Entity = { State.Registry, State.Registry.create() };
	auto& PState = Entity.emplace<FPlayerState>();
	PState.Position.Tile = { 9, 3, 0 };
	State.CameraTarget = Entity;
}

RE_API auto GameInitialize(FGameMemory& Memory) -> void
{
	// TODO(Rafis): Assert Memory is valid

	// Map game state to the memory
	FGameState& State = *(FGameState*)Memory.PermanentStorage;
	// Construct game state in place
	new (&State) FGameState();

	State.xOffset = 0;
	State.yOffset = 0;
	State.OffsetSpeed = 2 * 255.f;

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

	World.ScreenTileDim = glm::uvec2(21, 15);
	World.ScreenTileDimHalf = World.ScreenTileDim / glm::uvec2(2);
	u32 ScreenZ = 0;
	auto Screen = glm::uvec2(0);
	for (u32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex)
	{
		u32 RandomValue = rand() % 3;
		for (u32 TileY = 0; TileY < World.ScreenTileDim.y; ++TileY)
		{
			for (u32 TileX = 0; TileX < World.ScreenTileDim.x; ++TileX)
			{
				Tilemap::ETileType TileValue = Tilemap::ETileType::Empty;
				if (TileY == 0 || TileY == World.ScreenTileDim.y - 1)
				{
					TileValue = Tilemap::ETileType::Wall;
					if (TileX >= World.ScreenTileDimHalf.x - 1 && TileX <= World.ScreenTileDimHalf.x + 1)
					{
						TileValue = Tilemap::ETileType::Empty;
					}
				}

				if (TileX == 0 || TileX == World.ScreenTileDim.x - 1)
				{
					TileValue = Tilemap::ETileType::Wall;
					if (TileY >= World.ScreenTileDimHalf.y - 1 && TileY <= World.ScreenTileDimHalf.y + 1)
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

				auto TilePosition = Tilemap::FTilePosition(Screen * World.ScreenTileDim + glm::uvec2(TileX, TileY), ScreenZ);
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

	State.PlayerAcceleration = 150.0f;
	State.PlayerFrictionCoeficient = 20.f;
	State.PlayerSize = glm::vec2{ 0.65f, 0.9f };

	AddPlayer(State);

	State.CameraPosition.Tile = glm::uvec3(World.ScreenTileDimHalf, 0);
	State.CameraPosition.Local = glm::vec2(0.0f, 0.0f);

	State.BackgroundTexture.Pixels = (u32*)Memory.TempStorage;
	LoadImage(State.BackgroundTexture, "test/background.png");
	State.BgSprite = FSprite{ .Texture = &State.BackgroundTexture, .Max = { State.BackgroundTexture.Width, State.BackgroundTexture.Height } };

	State.PlayerTexture.Pixels =
		(u32*)((u8*)Memory.TempStorage + State.BackgroundTexture.Width * State.BackgroundTexture.Height * State.BackgroundTexture.Channels);
	LoadImage(State.PlayerTexture, "test/link_atlas.png");

	u32 Base = 740 * 2 + 1;
	// Player Idle
	{
		glm::uvec2 BaseOffset = { Base, 21 };
		glm::uvec2 BaseSize = { 24, 33 };
		glm::uvec2 BasePivot = BaseSize / glm::uvec2(2);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteIdle.size(); i++)
		{
			State.PlayerSpriteIdle[i] = BaseSprite;
			State.PlayerSpriteIdle[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteIdle[i].Max = State.PlayerSpriteIdle[i].Min + BaseSize;
		}
	}

	// Player move down
	{
		glm::uvec2 BaseOffset = { 324 + Base, 21 };
		glm::uvec2 BaseSize = { 23, 35 };
		glm::uvec2 BasePivot = BaseSize / glm::uvec2(2);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteMoveDown.size(); i++)
		{
			State.PlayerSpriteMoveDown[i] = BaseSprite;
			State.PlayerSpriteMoveDown[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteMoveDown[i].Max = State.PlayerSpriteMoveDown[i].Min + BaseSize;
		}
	}

	// Player move right
	{
		glm::uvec2 BaseOffset = { 324 + Base, 21 + 1 + 35 };
		glm::uvec2 BaseSize = { 24, 33 };
		glm::uvec2 BasePivot = BaseSize / glm::uvec2(2);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteMoveRight.size(); i++)
		{
			State.PlayerSpriteMoveRight[i] = BaseSprite;
			State.PlayerSpriteMoveRight[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteMoveRight[i].Max = State.PlayerSpriteMoveRight[i].Min + BaseSize;
		}
	}

	// Player move left
	{
		glm::uvec2 BaseOffset = { 324 + Base, 21 + 1 + 35 };
		glm::uvec2 BaseSize = { 24, 33 };
		glm::uvec2 BasePivot = BaseSize / glm::uvec2(2);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteMoveLeft.size(); i++)
		{
			State.PlayerSpriteMoveLeft[i] = BaseSprite;
			State.PlayerSpriteMoveLeft[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteMoveLeft[i].Max = State.PlayerSpriteMoveLeft[i].Min + BaseSize;
			State.PlayerSpriteMoveLeft[i].Min.x += BaseSize.x;
			State.PlayerSpriteMoveLeft[i].Max.x -= BaseSize.x;
		}
	}

	// Player move up
	{
		glm::uvec2 BaseOffset = { 324 + Base, 21 + 1 + 35 + 1 + 33 };
		glm::uvec2 BaseSize = { 26, 36 };
		glm::uvec2 BasePivot = BaseSize / glm::uvec2(2);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteMoveUp.size(); i++)
		{
			State.PlayerSpriteMoveUp[i] = BaseSprite;
			State.PlayerSpriteMoveUp[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteMoveUp[i].Max = State.PlayerSpriteMoveUp[i].Min + BaseSize;
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

auto DrawImage(FOffscreenBuffer& Buffer, glm::vec2 InMin, glm::vec2 InMax, const FSprite& Sprite) -> void
{
	// TODO(Rafis): assert on (Min.x >= Max.x || Min.y >= Max.y)

	auto SpriteXMin = Sprite.Min.x;
	auto SpriteXMax = Sprite.Max.x;
	if (Sprite.Min.x > Sprite.Max.x)
	{
		SpriteXMin = Sprite.Max.x;
		SpriteXMax = Sprite.Min.x;
	}

	glm::ivec2 Min = glm::round(InMin);
	glm::ivec2 Max = glm::round(InMax);

	Min -= Sprite.Pivot;
	Max -= Sprite.Pivot;

	auto OffsetSource = Min;

	Min = glm::clamp(Min, glm::ivec2(0), glm::ivec2(Buffer.Width, Buffer.Height));
	Max = glm::clamp(Max, glm::ivec2(0), glm::ivec2(Buffer.Width, Buffer.Height));

	OffsetSource = Min - OffsetSource;

	u32 Pitch = Buffer.Width * Buffer.BytesPerPixel;
	u8* Row = Buffer.Backbuffer + Min.x * Buffer.BytesPerPixel + Min.y * Pitch;
	u32 BufferX = Min.x;
	u32 BufferY = Min.y;

	for (u32 Y = Sprite.Min.y + OffsetSource.y; Y < Sprite.Max.y && Y < Sprite.Texture->Height && BufferY < Max.y; ++Y)
	{
		u32* Pixel = (u32*)Row;
		BufferX = Min.x;
		for (u32 X = SpriteXMin + OffsetSource.x; X < SpriteXMax && X < Sprite.Texture->Width && BufferX < Max.x; ++X)
		{
			u32 Color = Sprite.Texture->Pixels[Y * Sprite.Texture->Width + X];

			if (Sprite.Min.x > Sprite.Max.x)
			{
				auto offset = Sprite.Max.x + (Sprite.Min.x - X) - 1;
				Color = Sprite.Texture->Pixels[Y * Sprite.Texture->Width + offset];
			}

			u8 alpha = Color >> 24 & 0xFF;
			// HACK: Sprites have this color as background, set it transparent on load?
			if (Color == 4283072470)
			{
				alpha = 0;
			}

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
			BufferX++;
		}
		Row += Pitch;
		BufferY++;
	}
}

auto TestWall(f32 WallX, f32 PlayerRelX, f32 PlayerRelY, f32 PlayerDeltaX, f32 PlayerDeltaY, f32& tMin, f32 MinY, f32 MaxY) -> bool
{
	f32 tEpsilon = 0.01f;
	if (PlayerDeltaX != 0.0f)
	{
		f32 tResult = (WallX - PlayerRelX) / PlayerDeltaX;
		f32 Y = PlayerRelY + tResult * PlayerDeltaY;
		if (tResult >= 0.0f && tMin > tResult)
		{
			bool b1 = Y >= MinY;
			bool b2 = Y <= MaxY;
			if (Y >= MinY && Y <= MaxY)
			{
				tMin = glm::max(0.0f, tResult - tEpsilon);
				return true;
			}
		}
	}
	return false;
}

auto PlayerMove(FGameState& State, FPlayerState& Player, const Tilemap::FWorld& Tilemap, const FInputGamepad& Gamepad, f32 DeltaTime) -> void
{
	auto OldPlayerPos = Player.Position;

	// Calculate new player position
	{
		auto LeftStickAcceleration = glm::vec2(Gamepad.GamepadThumbstickLeft.X, Gamepad.GamepadThumbstickLeft.Y);
		if (Gamepad.GamepadDpadUp.bPressed)
		{
			LeftStickAcceleration.y = 1.0f;
		}
		if (Gamepad.GamepadDpadDown.bPressed)
		{
			LeftStickAcceleration.y = -1.0f;
		}
		if (Gamepad.GamepadDpadRight.bPressed)
		{
			LeftStickAcceleration.x = 1.0f;
		}
		if (Gamepad.GamepadDpadLeft.bPressed)
		{
			LeftStickAcceleration.x = -1.0f;
		}

		if (LeftStickAcceleration.x != 0 || LeftStickAcceleration.y != 0.0)
		{
			LeftStickAcceleration = glm::normalize(LeftStickAcceleration);
		}
		LeftStickAcceleration *= State.PlayerAcceleration;

		glm::vec2 frictionAcceleration = State.PlayerFrictionCoeficient * (-Player.Velocity);
		glm::vec2 Acceleration = LeftStickAcceleration + frictionAcceleration;

#if 0
		auto NewPlayerPositionDelta = 0.5f * Acceleration * DeltaTime * DeltaTime + Player.Velocity * DeltaTime;
		Player.Velocity = Acceleration * DeltaTime + Player.Velocity;

		auto NewPlayerPos = Tilemap::Offset(Tilemap, Player.Position, NewPlayerPositionDelta);
		auto NewPlayerPosLeft = Tilemap::Offset(Tilemap, Player.Position, NewPlayerPositionDelta + State.PlayerSize * glm::vec2(-0.5f, -0.5f));
		auto NewPlayerPosRight = Tilemap::Offset(Tilemap, Player.Position, NewPlayerPositionDelta + State.PlayerSize * glm::vec2(0.5f, -0.5f));

		if (Tilemap::IsTilemapPointEmpty(Tilemap, NewPlayerPos) && Tilemap::IsTilemapPointEmpty(Tilemap, NewPlayerPosLeft)
			&& Tilemap::IsTilemapPointEmpty(Tilemap, NewPlayerPosRight))
		{
			Player.Position = NewPlayerPos;
		}
#else
		glm::vec2 PlayerDelta = 0.5f * Acceleration * DeltaTime * DeltaTime + Player.Velocity * DeltaTime;
		Player.Velocity = Acceleration * DeltaTime + Player.Velocity;

		auto NewPlayerPos = Tilemap::Offset(Tilemap, Player.Position, PlayerDelta);

		u32 AbsTileZ = Player.Position.Tile.z;

		glm::uvec2 EntityTile = glm::ceil(State.PlayerSize / Tilemap.TileWidthInMeter);
		glm::uvec2 StartTile(glm::min(Player.Position.Tile.x, NewPlayerPos.Tile.x), glm::min(Player.Position.Tile.y, NewPlayerPos.Tile.y));
		glm::uvec2 EndTile(glm::max(Player.Position.Tile.x, NewPlayerPos.Tile.x), glm::max(Player.Position.Tile.y, NewPlayerPos.Tile.y));
		StartTile -= EntityTile;
		EndTile += EntityTile;

		glm::vec2 WallNormal(0.0f);
		f32 tRemain = 1.0f;
		for (u32 Iter = 0; Iter < 4 && tRemain > 0.0f; ++Iter)
		{
			f32 tMin = 1.0f;
			u32 AbsTileY = StartTile.y;
			for (u32 AbsTileY = StartTile.y; AbsTileY <= EndTile.y; ++AbsTileY)
			{
				for (u32 AbsTileX = StartTile.x; AbsTileX <= EndTile.x; ++AbsTileX)
				{
					auto TestTile = Tilemap::CreateTilemapPosition(AbsTileX, AbsTileY, AbsTileZ);
					auto TileID = Tilemap::GetTileValue(Tilemap, TestTile.Tile);
					if (!Tilemap::IsTileEmpty(TileID))
					{
						glm::vec2 Diameter = State.PlayerSize + Tilemap.TileWidthInMeter;
						glm::vec2 MinCorner = -0.5f * Diameter;
						glm::vec2 MaxCorner = 0.5f * Diameter;

						auto RelPlayerPosition = Tilemap::Subtract(Tilemap, Player.Position, TestTile);
						glm::vec2 Rel(RelPlayerPosition);

						if (TestWall(MinCorner.x, Rel.x, Rel.y, PlayerDelta.x, PlayerDelta.y, tMin, MinCorner.y, MaxCorner.y))
						{
							WallNormal = { -1.0f, 0.0f };
						}
						if (TestWall(MaxCorner.x, Rel.x, Rel.y, PlayerDelta.x, PlayerDelta.y, tMin, MinCorner.y, MaxCorner.y))
						{
							WallNormal = { 1.0f, 0.0f };
						}
						if (TestWall(MinCorner.y, Rel.y, Rel.x, PlayerDelta.y, PlayerDelta.x, tMin, MinCorner.x, MaxCorner.x))
						{
							WallNormal = { 0.0f, -1.0f };
						}
						if (TestWall(MaxCorner.y, Rel.y, Rel.x, PlayerDelta.y, PlayerDelta.x, tMin, MinCorner.x, MaxCorner.x))
						{
							WallNormal = { 0.0f, 1.0f };
						}
					}
				}
			}

			Player.Position = Tilemap::Offset(Tilemap, Player.Position, tMin * PlayerDelta);
			Player.Velocity = Player.Velocity - 1.0f * Player.Velocity * WallNormal * WallNormal;
			PlayerDelta = PlayerDelta - 1.0f * PlayerDelta * WallNormal * WallNormal;

			tRemain -= tMin * tRemain;
		}
#endif
	}

	// Calculate Z layer and camera position
	{
		if (!Tilemap::OnSameTile(Tilemap, OldPlayerPos, Player.Position))
		{
			auto TileID = Tilemap::GetTileValue(Tilemap, Player.Position.Tile);
			if (TileID == Tilemap::ETileType::StairDown)
			{
				Player.Position.Tile.z = 1;
			}
			else if (TileID == Tilemap::ETileType::StairUp)
			{
				Player.Position.Tile.z = 0;
			}
		}
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
		// State.PlayerPosition.Tile = glm::uvec3(9, 3, State.PlayerPosition.Tile.z);
		// State.PlayerPosition.Local = glm::vec2(0.0f, 0.0f);
		// State.PlayerAcceleration = (State.PlayerAcceleration == 4.0f) ? 8.0f : 4.0f;
		// State.PlayerAcceleration += 1.f;
	}

	State.PlayerSpriteIdleIndex += 8.0f * DeltaTime;
	auto PlayerView = State.Registry.view<FPlayerState>();
	for (auto [entity, Player] : PlayerView.each())
	{
		State.PlayerSpriteMoveIndex += glm::length(Player.Velocity) * DeltaTime;
		PlayerMove(State, Player, Tilemap, Gamepad, DeltaTime);
	}

	if (State.CameraTarget.entity() != entt::null && State.CameraTarget.any_of<FPlayerState>())
	{
		auto& Player = State.CameraTarget.get<FPlayerState>();
		State.CameraPosition.Tile = Tilemap::FTilePosition(
			(glm::uvec2(Player.Position.Tile) / World.ScreenTileDim) * World.ScreenTileDim + World.ScreenTileDimHalf, Player.Position.Tile.z);
	}

	// Clear screen
	// DrawRect(Buffer, glm::vec2{ 0.0f, 0.0f }, glm::vec2{ Buffer.Width, Buffer.Height }, glm::vec3{ 0.08f, 0.17f, 0.85f });

	DrawImage(Buffer, glm::vec2{ 0, 0 }, glm::vec2{ State.BgSprite.Texture->Width, State.BgSprite.Texture->Height }, State.BgSprite);

	i32 HalfTilesPerScreenHorizontal = (i32)(glm::floor(Buffer.Width / World.TileWidthInPixels) + 1) / 2;
	i32 HalfTilesPerScreenVertical = (i32)(glm::floor(Buffer.Height / World.TileWidthInPixels) + 1) / 2;

	// Draw tiles
	for (i32 RelRow = -HalfTilesPerScreenVertical; RelRow <= HalfTilesPerScreenVertical; ++RelRow)
	{
		for (i32 RelColumn = -HalfTilesPerScreenHorizontal; RelColumn <= HalfTilesPerScreenHorizontal; ++RelColumn)
		{
			auto Tile = Tilemap::FTilePosition(glm::ivec2(State.CameraPosition.Tile) + glm::ivec2{ RelColumn, RelRow }, State.CameraPosition.Tile.z);
			auto TileID = Tilemap::GetTileValue(Tilemap, Tile);

			if (TileID != Tilemap::ETileType::Invalid && TileID != Tilemap::ETileType::Empty)
			{
				auto DrawSize = glm::vec2(World.TileWidthInPixels) - World.Padding * 2.0f;
				auto DrawPosition = World.DrawPosition;
				// Account for the 0,0 position of the tile is at the center now
				DrawPosition.x -= Tilemap.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
				DrawPosition.y += Tilemap.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
				// Account for the player position as everything is drawn relative to the player
				// DrawPosition -= (State.PlayerPosition.Local * World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
				DrawPosition += (glm::vec2(RelColumn, RelRow) * World.TileWidthInPixels + World.Padding) * glm::vec2(1.0f, -1.0f);
				DrawPosition += DrawSize * glm::vec2(0.0f, -1.0f);

				auto Color = glm::vec3(0);
				switch (TileID)
				{
					case Tilemap::ETileType::Wall:
					{
						Color = glm::vec3(1);
						if (State.CameraPosition.Tile.z == 1)
						{
							Color = glm::vec3(0.07f, 0.15f, 0.18f);
						}
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
	for (auto [entity, Player] : PlayerView.each())
	{
		auto DrawSize = State.PlayerSize * World.PixelsPerMeter;
		auto DrawPosition = World.DrawPosition;
		DrawPosition -= (glm::vec2(State.CameraPosition.Tile) * World.TileWidthInPixels) * glm::vec2(1.0f, -1.0f);
		DrawPosition += (glm::vec2(Player.Position.Tile) * World.TileWidthInPixels) * glm::vec2(1.0f, -1.0f);
		DrawPosition += (Player.Position.Local * World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);

#if defined(RE_INTERNAL) && 0
		// Draw the tile player is on
		{
			auto DebugDrawSize = glm::vec2(World.TileWidthInPixels);
			auto DebugDrawPosition = DrawPosition;
			// Account for the 0,0 position of the tile is at the center now
			DebugDrawPosition.x -= Tilemap.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
			DebugDrawPosition.y += Tilemap.TileWidthInMeter * World.PixelsPerMeter * 0.5f;
			// Account for the player position as everything is drawn relative to the player
			DebugDrawPosition -= (Player.Position.Local * World.PixelsPerMeter) * glm::vec2(1.0f, -1.0f);
			DebugDrawPosition += DebugDrawSize * glm::vec2(0.0f, -1.0f);
			DrawRect(Buffer, DebugDrawPosition, DebugDrawPosition + DebugDrawSize, glm::vec3{ 0.3f, 0.8f, 0.1f });
		}
#endif

#if defined(RE_INTERNAL) && 1
		// Draw player collision
		DrawRect(Buffer, DrawPosition - DrawSize * glm::vec2(0.5f), DrawPosition + DrawSize * glm::vec2(0.5f), glm::vec3(0.65f, 0.08f, 0.15f));
#endif
		u32 index = 0;
		if (glm::abs(Player.Velocity.x) < 0.1f && glm::abs(Player.Velocity.y) < 0.1f)
		{
			index = (u32)(glm::round(State.PlayerSpriteIdleIndex)) % State.PlayerSpriteIdle.size();
			DrawImage(Buffer, DrawPosition, DrawPosition + DrawSize, State.PlayerSpriteIdle[index]);
		}
		else
		{
			if (glm::abs(Player.Velocity.x) > glm::abs(Player.Velocity.y))
			{
				if (Player.Velocity.x > 0)
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveRight.size();
					DrawImage(Buffer, DrawPosition, DrawPosition + DrawSize, State.PlayerSpriteMoveRight[index]);
				}
				else
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveLeft.size();
					DrawImage(Buffer, DrawPosition, DrawPosition + DrawSize, State.PlayerSpriteMoveLeft[index]);
				}
			}
			else
			{
				if (Player.Velocity.y > 0)
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveUp.size();
					DrawImage(Buffer, DrawPosition, DrawPosition + DrawSize, State.PlayerSpriteMoveUp[index]);
				}
				else
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveDown.size();
					DrawImage(Buffer, DrawPosition, DrawPosition + DrawSize, State.PlayerSpriteMoveDown[index]);
				}
			}
		}
	}

	// Center of the screen
	{
		DrawRect(Buffer, World.DrawPosition - glm::vec2(4.f), World.DrawPosition + glm::vec2(4.f), glm::vec3(0.15f, 0.08f, 0.10f));
	}
}
