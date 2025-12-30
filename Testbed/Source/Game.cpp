#include <Platform.h>

#include "GameMath.h"
#include "Memory.h"
#include "Tilemap.cpp"

#include <filesystem>
#include <iostream>

#include <entt/entt.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

namespace Comp
{
	struct FPlayer
	{
		u32 ControllerIndex;
	};

	struct FTransform
	{
		glm::vec3 Position;
	};

	struct FMovement
	{
		glm::vec2 Velocity;
		f32 PlayerAccelerationRate;
	};

	struct FSize
	{
		glm::vec2 Size;
	};

	struct FDraw
	{
		glm::vec3 Color;
		FSprite* Sprite;
	};

	struct FTileTransform
	{
		Tilemap::FTilemapPosition Position;
	};

	struct FTeleport
	{
		i32 AbsTileZ;
	};

	struct FCollider
	{
		glm::vec2 Size;
		bool bCollide;
	};
} // namespace Comp

struct FGameState
{
	f32 xOffset;
	f32 yOffset;
	f32 OffsetSpeed;

	Memory::Arena::FMemoryArena WorldArena;
	FWorld* World;

	entt::handle CameraTarget;
	Tilemap::FTilemapPosition CameraPosition;

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

auto AddPlayer(FGameState& State, glm::uvec3 TilePos) -> void
{
	entt::handle Entity = { State.Registry, State.Registry.create() };

	auto& TileTransformComp = Entity.emplace<Comp::FTileTransform>();
	TileTransformComp.Position.Tile = TilePos;

	auto& MovementComp = Entity.emplace<Comp::FMovement>();
	MovementComp.PlayerAccelerationRate = 250.0f;

	auto& SizeComp = Entity.emplace<Comp::FSize>();
	SizeComp.Size = glm::vec2{ 0.65f, 0.9f };

	auto& DrawComp = Entity.emplace<Comp::FDraw>();
	DrawComp.Color = glm::vec3(0.8f, 0.15f, 0.85f);

	auto& ColliderComp = Entity.emplace<Comp::FCollider>();
	ColliderComp.Size = SizeComp.Size;
	ColliderComp.bCollide = true;

	auto& PlayerComp = Entity.emplace<Comp::FPlayer>();

	if (State.CameraTarget.entity() == entt::null)
	{
		State.CameraTarget = Entity;
	}
}

auto AddWall(FGameState& State, Tilemap::FTilePosition Pos, glm::vec2 Size, Tilemap::ETileType Type) -> void
{
	entt::handle Entity = { State.Registry, State.Registry.create() };

	auto& TileTransformComp = Entity.emplace<Comp::FTileTransform>();
	TileTransformComp.Position.Tile = Pos;

	auto& SizeComp = Entity.emplace<Comp::FSize>();
	SizeComp.Size = Size;

	auto& DrawComp = Entity.emplace<Comp::FDraw>();

	if (Type == Tilemap::ETileType::StairDown)
	{
		auto& TeleportComp = Entity.emplace<Comp::FTeleport>();
		TeleportComp.AbsTileZ = -1;
		auto& ColliderComp = Entity.emplace<Comp::FCollider>();
		ColliderComp.Size = SizeComp.Size;
		ColliderComp.bCollide = false;
	}
	else if (Type == Tilemap::ETileType::StairUp)
	{
		auto& TeleportComp = Entity.emplace<Comp::FTeleport>();
		TeleportComp.AbsTileZ = +1;
		auto& ColliderComp = Entity.emplace<Comp::FCollider>();
		ColliderComp.Size = SizeComp.Size;
		ColliderComp.bCollide = false;
	}
	else if (Type == Tilemap::ETileType::Wall)
	{
		auto& ColliderComp = Entity.emplace<Comp::FCollider>();
		ColliderComp.Size = SizeComp.Size;
		ColliderComp.bCollide = true;
	}

	switch (Type)
	{
		case Tilemap::ETileType::Wall:
		{
			DrawComp.Color = glm::vec3(1);
			if (Pos.z % 2 == 1)
			{
				DrawComp.Color = glm::vec3(0.07f, 0.15f, 0.18f);
			}
			break;
		}
		case Tilemap::ETileType::Uninitialized:
		{
			DrawComp.Color = glm::vec3(0.5f);
			break;
		}
		case Tilemap::ETileType::StairUp:
		{
			DrawComp.Color = glm::vec3(1, 0, 0);
			break;
		}
		case Tilemap::ETileType::StairDown:
		{
			DrawComp.Color = glm::vec3(0, 0, 1);
			break;
		}
		default:
		{
			DrawComp.Color = glm::vec3(0.f);
			break;
		}
	}
}

auto GenerateTilemap(FGameState& State, u32 ScreenCount) -> void
{
	struct FTilemapScreen
	{
		bool doorTop = false;
		bool doorRight = false;
		bool doorBottom = false;
		bool doorLeft = false;
		bool stairUp = false;
		bool stairDown = false;
		u32 ScreenX = 0;
		u32 ScreenY = 0;
		u32 ScreenZ = 0;
	};

	std::vector<FTilemapScreen> Screens;
	Screens.resize(ScreenCount);

	auto ScreenPos = glm::uvec2(0);
	u32 ScreenZ = 0;
	for (u32 ScreenIndex = 0; ScreenIndex < ScreenCount; ++ScreenIndex)
	{
		auto& TScreen = Screens[ScreenIndex];
		TScreen.ScreenX = ScreenPos.x;
		TScreen.ScreenY = ScreenPos.y;
		TScreen.ScreenZ = ScreenZ;

		u32 RandomValue;
		if (ScreenIndex > 0 && (Screens[ScreenIndex - 1].stairDown || Screens[ScreenIndex - 1].stairUp))
		{
			RandomValue = rand() % 2;
		}
		else
		{
			RandomValue = rand() % 3;
		}

		if (RandomValue == 2 && ScreenIndex + 1 < ScreenCount)
		{
			if (ScreenZ == 0)
			{
				TScreen.stairUp = true;
				Screens[ScreenIndex + 1].stairDown = true;
			}
			else
			{
				TScreen.stairDown = true;
				Screens[ScreenIndex + 1].stairUp = true;
			}

			ScreenZ = (ScreenZ + 1) % 2;
		}
		else if (RandomValue == 0)
		{
			// right door
			if (ScreenIndex + 1 < ScreenCount)
			{
				TScreen.doorRight = true;
				Screens[ScreenIndex + 1].doorLeft = true;
				ScreenPos.x += 1;
			}
		}
		else
		{
			// top door
			if (ScreenIndex + 1 < ScreenCount)
			{
				TScreen.doorTop = true;
				Screens[ScreenIndex + 1].doorBottom = true;
				ScreenPos.y += 1;
			}
		}
	}

	auto ScreenTileDim = State.World->ScreenTileDim;
	auto ScreenTileDimHalf = State.World->ScreenTileDimHalf;
	for (auto& Screen : Screens)
	{
		for (u32 TileY = 0; TileY < ScreenTileDim.y; ++TileY)
		{
			for (u32 TileX = 0; TileX < ScreenTileDim.x; ++TileX)
			{
				auto TilePosition =
					Tilemap::FTilePosition(glm::uvec2(Screen.ScreenX, Screen.ScreenY) * ScreenTileDim + glm::uvec2(TileX, TileY), Screen.ScreenZ);
				Tilemap::ETileType TileValue = Tilemap::ETileType::Empty;

				if (TileY == 0 || TileY == ScreenTileDim.y - 1)
				{
					TileValue = Tilemap::ETileType::Wall;
					if (Screen.doorBottom && TileY == 0 && TileX >= ScreenTileDimHalf.x - 1 && TileX <= ScreenTileDimHalf.x + 1)
					{
						TileValue = Tilemap::ETileType::Empty;
					}
					if (Screen.doorTop && TileY == (ScreenTileDim.y - 1) && TileX >= ScreenTileDimHalf.x - 1 && TileX <= ScreenTileDimHalf.x + 1)
					{
						TileValue = Tilemap::ETileType::Empty;
					}
				}

				if (TileX == 0 || TileX == ScreenTileDim.x - 1)
				{
					TileValue = Tilemap::ETileType::Wall;
					if (Screen.doorRight && TileX == (ScreenTileDim.x - 1) && TileY >= ScreenTileDimHalf.y - 1 && TileY <= ScreenTileDimHalf.y + 1)
					{
						TileValue = Tilemap::ETileType::Empty;
					}
					if (Screen.doorLeft && TileX == 0 && TileY >= ScreenTileDimHalf.y - 1 && TileY <= ScreenTileDimHalf.y + 1)
					{
						TileValue = Tilemap::ETileType::Empty;
					}
				}

				if (TileX == ScreenTileDimHalf.x - 1 && TileY == ScreenTileDimHalf.y - 1 && Screen.stairDown)
				{
					TileValue = Tilemap::ETileType::StairDown;
				}

				if (TileX == ScreenTileDimHalf.x + 1 && TileY == ScreenTileDimHalf.y + 1 && Screen.stairUp)
				{
					TileValue = Tilemap::ETileType::StairUp;
				}

				Tilemap::SetTileValue(State.WorldArena, *State.World->Tilemap, TilePosition, TileValue);
				if (TileValue != Tilemap::ETileType::Empty)
					AddWall(State, TilePosition, glm::vec2(State.World->Tilemap->TileWidthInMeter), TileValue);
			}
		}
	}
}

auto SetCamera(FGameState& State, Tilemap::FTilemapPosition NewCamPos)
{
	if (State.CameraPosition.Tile != NewCamPos.Tile || State.CameraPosition.Local != NewCamPos.Local)
	{
		State.CameraPosition = NewCamPos;
		auto CameraBounds = Math::RectFromCenter(glm::vec2(State.CameraPosition.Tile), 3.0f * glm::vec2(State.World->ScreenTileDimHalf));

		auto TileView = State.Registry.view<Comp::FTileTransform>();
		for (auto [Entity, TileTransform] : TileView.each())
		{
			auto Handle = entt::handle(State.Registry, Entity);
			if (Math::IsInRect(CameraBounds, glm::vec2(TileTransform.Position.Tile)))
			{
				Handle.get_or_emplace<Comp::FTransform>();
			}
			else if (Handle.any_of<Comp::FTransform>())
			{
				Handle.remove<Comp::FTransform>();
			}
		}

		auto View = State.Registry.view<Comp::FTransform, Comp::FTileTransform>();
		for (auto [Entity, Transform, TileTransform] : View.each())
		{
			Tilemap::MapToCameraSpace(*State.World->Tilemap, State.CameraPosition, TileTransform.Position, Transform.Position);
		}
	}
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

	GenerateTilemap(State, 10);

	State.PlayerFrictionCoeficient = 20.f;

	AddPlayer(State, { 9, 9, 0 });

	SetCamera(State, { glm::uvec3(World.ScreenTileDimHalf, 0), glm::vec2(0.0f, 0.0f) });

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

auto PlayerMove(FGameState& State, const FInputGamepad& Gamepad, f32 DeltaTime) -> void
{
	auto MoveView = State.Registry.view<Comp::FTransform, Comp::FMovement, Comp::FCollider, Comp::FTileTransform>();
	auto ColView = State.Registry.view<Comp::FTransform, Comp::FCollider, Comp::FTileTransform>();

	// Calculate new player position
	for (auto [Entity, Transform, Movement, Collider, TileTransform] : MoveView.each())
	{
		// TODO(Rafis): TEMP, this should handle animation system
		if (State.Registry.any_of<Comp::FPlayer>(Entity))
		{
			State.PlayerSpriteMoveIndex += glm::length(Movement.Velocity) * DeltaTime;
		}

		auto OldPosition = Transform.Position;

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
		LeftStickAcceleration *= Movement.PlayerAccelerationRate;

		glm::vec2 frictionAcceleration = State.PlayerFrictionCoeficient * (-Movement.Velocity);
		glm::vec2 Acceleration = LeftStickAcceleration + frictionAcceleration;

		glm::vec2 MoveDelta = 0.5f * Acceleration * DeltaTime * DeltaTime + Movement.Velocity * DeltaTime;
		Movement.Velocity = Acceleration * DeltaTime + Movement.Velocity;

		glm::vec2 WallNormal(0.0f);
		f32 tRemain = 1.0f;
		entt::handle HitEntity;
		for (u32 Iter = 0; Iter < 4 && tRemain > 0.0f; ++Iter)
		{
			f32 tMin = 1.0f;
			for (auto [ColEntity, ColTransform, ColCollider, ColTileTransform] : ColView.each())
			{
				if (Entity != ColEntity)
				{
					if (TileTransform.Position.Tile.z == ColTileTransform.Position.Tile.z)
					{
						if (ColCollider.bCollide)
						{
							glm::vec2 Diameter = Collider.Size + ColCollider.Size;
							glm::vec2 MinCorner = -0.5f * Diameter;
							glm::vec2 MaxCorner = 0.5f * Diameter;

							auto Rel = Transform.Position - ColTransform.Position;

							if (TestWall(MinCorner.x, Rel.x, Rel.y, MoveDelta.x, MoveDelta.y, tMin, MinCorner.y, MaxCorner.y))
							{
								WallNormal = { -1.0f, 0.0f };
								HitEntity = entt::handle(State.Registry, ColEntity);
							}
							if (TestWall(MaxCorner.x, Rel.x, Rel.y, MoveDelta.x, MoveDelta.y, tMin, MinCorner.y, MaxCorner.y))
							{
								WallNormal = { 1.0f, 0.0f };
								HitEntity = entt::handle(State.Registry, ColEntity);
							}
							if (TestWall(MinCorner.y, Rel.y, Rel.x, MoveDelta.y, MoveDelta.x, tMin, MinCorner.x, MaxCorner.x))
							{
								WallNormal = { 0.0f, -1.0f };
								HitEntity = entt::handle(State.Registry, ColEntity);
							}
							if (TestWall(MaxCorner.y, Rel.y, Rel.x, MoveDelta.y, MoveDelta.x, tMin, MinCorner.x, MaxCorner.x))
							{
								WallNormal = { 0.0f, 1.0f };
								HitEntity = entt::handle(State.Registry, ColEntity);
							}
						}
						else
						{
							glm::vec2 Diameter = Collider.Size + ColCollider.Size;
							glm::vec2 MinCorner = -0.5f * Diameter;
							glm::vec2 MaxCorner = 0.5f * Diameter;

							auto Rel = Transform.Position - ColTransform.Position;
							if (glm::length(Rel) <= glm::length(Diameter) / 2.0f)
							{
								HitEntity = entt::handle(State.Registry, ColEntity);
							}
						}
					}
				}
			}

			Transform.Position += glm::vec3(tMin * MoveDelta, 0.0f);
			if (HitEntity.entity() != entt::null)
			{
				Movement.Velocity = Movement.Velocity - 1.0f * Movement.Velocity * WallNormal * WallNormal;
				MoveDelta = MoveDelta - 1.0f * MoveDelta * WallNormal * WallNormal;

				tRemain -= tMin * tRemain;

				if (HitEntity.any_of<Comp::FTeleport>())
				{
					auto& TeleportComp = HitEntity.get<Comp::FTeleport>();
					Transform.Position.z += TeleportComp.AbsTileZ;
					break;
				}
			}
			else
			{
				break;
			}
		}

		Tilemap::MapToTileSpace(*State.World->Tilemap, State.CameraPosition, Transform.Position, TileTransform.Position);
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

	PlayerMove(State, Gamepad, DeltaTime);

	auto MoveView = State.Registry.view<Comp::FMovement, Comp::FDraw>();
	for (auto [entity, Movement, Draw] : MoveView.each())
	{
		u32 index = 0;
		if (glm::abs(Movement.Velocity.x) < 0.1f && glm::abs(Movement.Velocity.y) < 0.1f)
		{
			index = (u32)(glm::round(State.PlayerSpriteIdleIndex)) % State.PlayerSpriteIdle.size();
			Draw.Sprite = &State.PlayerSpriteIdle[index];
		}
		else
		{
			if (glm::abs(Movement.Velocity.x) > glm::abs(Movement.Velocity.y))
			{
				if (Movement.Velocity.x > 0)
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveRight.size();
					Draw.Sprite = &State.PlayerSpriteMoveRight[index];
				}
				else
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveLeft.size();
					Draw.Sprite = &State.PlayerSpriteMoveLeft[index];
				}
			}
			else
			{
				if (Movement.Velocity.y > 0)
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveUp.size();
					Draw.Sprite = &State.PlayerSpriteMoveUp[index];
				}
				else
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveDown.size();
					Draw.Sprite = &State.PlayerSpriteMoveDown[index];
				}
			}
		}
	}

	if (State.CameraTarget.entity() != entt::null && State.CameraTarget.all_of<Comp::FTransform, Comp::FTileTransform>())
	{
		auto& TileTransform = State.CameraTarget.get<Comp::FTileTransform>();

		// Switch between smooth scrolling and fixed camera position
#if 1
		auto NewCamPos = Tilemap::FTilePosition(
			(glm::uvec2(TileTransform.Position.Tile) / World.ScreenTileDim) * World.ScreenTileDim + World.ScreenTileDimHalf, TileTransform.Position.Tile.z);
		SetCamera(State, { NewCamPos, { 0.0f, 0.0f } });
#else
		SetCamera(State, TileTransform.Position);
#endif
	}

	// Clear screen
	DrawRect(Buffer, glm::vec2{ 0.0f, 0.0f }, glm::vec2{ Buffer.Width, Buffer.Height }, glm::vec3{ 0.08f, 0.17f, 0.85f });

	DrawImage(Buffer, glm::vec2{ 0, 0 }, glm::vec2{ State.BgSprite.Texture->Width, State.BgSprite.Texture->Height }, State.BgSprite);

	// Collision draw
#if defined(RE_INTERNAL) && 0
	auto DrawDebugView = State.Registry.view<Comp::FTransform, Comp::FTileTransform, Comp::FCollider>();
	for (auto [entity, Transform, TileTransform, Collider] : DrawDebugView.each())
	{
		if (TileTransform.Position.Tile.z == State.CameraPosition.Tile.z)
		{
			auto ColDrawSize = Collider.Size * World.PixelsPerMeter;
			auto DrawPosition = World.DrawPosition + glm::vec2(Transform.Position) * World.PixelsPerMeter * glm::vec2(1.0f, -1.0f);
			DrawRect(Buffer, DrawPosition - ColDrawSize * glm::vec2(0.5f), DrawPosition + ColDrawSize * glm::vec2(0.5f), glm::vec3(0.25f, 0.65f, 0.1f));
		}
	}
#endif

	// main draw
	auto DrawView = State.Registry.view<Comp::FTransform, Comp::FTileTransform, Comp::FSize, Comp::FDraw>();
	for (auto [entity, Transform, TileTransform, Size, Draw] : DrawView.each())
	{
		if (TileTransform.Position.Tile.z == State.CameraPosition.Tile.z)
		{
			auto DrawSize = Size.Size * World.PixelsPerMeter - glm::vec2(2);
			auto DrawPosition = World.DrawPosition + glm::vec2(Transform.Position) * World.PixelsPerMeter * glm::vec2(1.0f, -1.0f);

			if (!Draw.Sprite)
			{
				DrawRect(Buffer, DrawPosition - DrawSize * glm::vec2(0.5f), DrawPosition + DrawSize * glm::vec2(0.5f), Draw.Color);
			}
			else
			{
				DrawImage(Buffer, DrawPosition, DrawPosition + DrawSize, *Draw.Sprite);
			}
		}
	}

	// Center of the screen
	{
		DrawRect(Buffer, World.DrawPosition - glm::vec2(4.f), World.DrawPosition + glm::vec2(4.f), glm::vec3(0.15f, 0.08f, 0.10f));
	}
}
