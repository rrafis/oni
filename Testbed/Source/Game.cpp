#include <Platform.h>

#include "Components.h"
#include "GameMath.h"
#include "Memory.h"
#include "SimRegion.h"
#include "World.h"

#include <filesystem>
#include <iostream>

// #define ENTT_ID_TYPE u64
#include <entt/entt.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct FGameState
{
	f32 xOffset;
	f32 yOffset;
	f32 OffsetSpeed;

	Memory::Arena::FMemoryArena WorldArena;
	Engine::FWorld World;

	entt::handle CameraTarget;
	Engine::FWorldPosition CameraPosition;

	f32 PlayerFrictionCoeficient;
	f32 PlayerSpriteIdleIndex;
	f32 PlayerSpriteMoveIndex;

	FImage BackgroundTexture;
	FImage PlayerTexture;
	FImage WallTexture;

	FSprite BgSprite;
	std::array<FSprite, 4> PlayerSpriteIdle;
	FSprite WallSprite;

	std::array<FSprite, 6> PlayerSpriteMoveDown;
	std::array<FSprite, 6> PlayerSpriteMoveRight;
	std::array<FSprite, 6> PlayerSpriteMoveLeft;
	std::array<FSprite, 6> PlayerSpriteMoveUp;

	entt::registry Registry;

	Engine::FSimRegion ActiveRegion;
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

		// HACK: Sprites have this color as background, set it transparent on load?
		if (r == 74 && g == 127 && b == 214 && a == 255)
		{
			a = 0;
		}

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

auto AddPlayer(FGameState& State, const glm::ivec3& TilePos) -> void
{
	entt::handle Entity = { State.Registry, State.Registry.create() };

	auto& WorldTransformComp = Entity.emplace<Comp::FWorldTransform>();
	WorldTransformComp.Position = Engine::CreateWorldPosition(State.World, TilePos);
	Engine::SetLocation(State.World, Entity, WorldTransformComp.Position.Chunk);

	auto& TransformComp = Entity.emplace<Comp::FTransform>();

	auto& MovementComp = Entity.emplace<Comp::FMovement>();
	MovementComp.PlayerAccelerationRate = 250.0f;

	auto& SizeComp = Entity.emplace<Comp::FSize>();
	SizeComp.Size = glm::vec2{ 1.0f, 1.3f };

	auto& DrawComp = Entity.emplace<Comp::FDrawSprite>();

	auto& ColliderComp = Entity.emplace<Comp::FCollider>();
	ColliderComp.Size = SizeComp.Size;
	ColliderComp.bCollide = true;

	auto& PlayerComp = Entity.emplace<Comp::FPlayer>();

	if (State.CameraTarget.entity() == entt::null)
	{
		State.CameraTarget = Entity;
	}
}

auto AddWall(FGameState& State, const glm::ivec3& TilePos, const glm::vec2& Size, Engine::EObjectType Type) -> void
{
	entt::handle Entity = { State.Registry, State.Registry.create() };

	auto& WorldTransformComp = Entity.emplace<Comp::FWorldTransform>();
	WorldTransformComp.Position = Engine::CreateWorldPosition(State.World, TilePos);
	Engine::SetLocation(State.World, Entity, WorldTransformComp.Position.Chunk);

	auto& TransformComp = Entity.emplace<Comp::FTransform>();

	auto& SizeComp = Entity.emplace<Comp::FSize>();
	SizeComp.Size = Size;

	auto& DrawComp = Entity.emplace<Comp::FDrawSprite>();
	DrawComp.Sprite = State.WallSprite;

	if (Type == Engine::EObjectType::StairDown)
	{
		auto& TeleportComp = Entity.emplace<Comp::FTeleport>();
		TeleportComp.Z = -1;
		auto& ColliderComp = Entity.emplace<Comp::FCollider>();
		ColliderComp.Size = SizeComp.Size;
		ColliderComp.bCollide = false;
	}
	else if (Type == Engine::EObjectType::StairUp)
	{
		auto& TeleportComp = Entity.emplace<Comp::FTeleport>();
		TeleportComp.Z = +1;
		auto& ColliderComp = Entity.emplace<Comp::FCollider>();
		ColliderComp.Size = SizeComp.Size;
		ColliderComp.bCollide = false;
	}
	else if (Type != Engine::EObjectType::Uninitialized && Type != Engine::EObjectType::Empty && Type != Engine::EObjectType::Invalid)
	{
		auto& ColliderComp = Entity.emplace<Comp::FCollider>();
		ColliderComp.Size = SizeComp.Size;
		ColliderComp.bCollide = true;
	}

	switch (Type)
	{
		case Engine::EObjectType::WallTopRight:
		{
			DrawComp.Sprite.Min.x = 3 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallTopLeft:
		{
			DrawComp.Sprite.Min.x = 6 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallBottomRight:
		{
			DrawComp.Sprite.Min.x = 9 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallBottomLeft:
		{
			DrawComp.Sprite.Max.x = 9 * 32;
			DrawComp.Sprite.Min.x += DrawComp.Sprite.Max.x + 32;
			break;
		}
		case Engine::EObjectType::WallHorizontalCenter:
		{
			DrawComp.Sprite.Min.x = 5 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallHorizontalTopLeft:
		{
			DrawComp.Sprite.Min.x = 9 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallHorizontalTopRight:
		{
			DrawComp.Sprite.Max.x = 9 * 32;
			DrawComp.Sprite.Min.x += DrawComp.Sprite.Max.x + 32;
			break;
		}
		case Engine::EObjectType::WallHorizontalBottomLeft:
		{
			DrawComp.Sprite.Min.x = 3 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallHorizontalBottomRight:
		{
			DrawComp.Sprite.Min.x = 6 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallVerticalCenter:
		{
			DrawComp.Sprite.Min.x = 10 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallVerticalBottomLeft:
		{
			DrawComp.Sprite.Max.x = 9 * 32;
			DrawComp.Sprite.Min.x += DrawComp.Sprite.Max.x + 32;
			break;
		}
		case Engine::EObjectType::WallVerticalBottomRight:
		{
			DrawComp.Sprite.Min.x = 9 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallVerticalTopRight:
		{
			DrawComp.Sprite.Min.x = 3 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::WallVerticalTopLeft:
		{
			DrawComp.Sprite.Min.x = 6 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::StairUp:
		{
			DrawComp.Sprite.Min.x = 16 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			break;
		}
		case Engine::EObjectType::StairDown:
		{
			DrawComp.Sprite.Min.x = 16 * 32;
			DrawComp.Sprite.Max.x += DrawComp.Sprite.Min.x;
			DrawComp.Sprite.Min.y = 32;
			DrawComp.Sprite.Max.y = 0;
			DrawComp.Sprite.Pivot.y = 20;
			break;
		}
		default:
		{
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
		glm::ivec2 ScreenPos;
		i32 ScreenZ = 0;
	};

	std::vector<FTilemapScreen> Screens;
	Screens.resize(ScreenCount);

	auto ScreenPos = glm::ivec2(0);
	i32 ScreenZ = 0;
	for (u32 ScreenIndex = 0; ScreenIndex < ScreenCount; ++ScreenIndex)
	{
		auto& TScreen = Screens[ScreenIndex];
		TScreen.ScreenPos = ScreenPos;
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
#define TM_POS 1
		else if (RandomValue == 0)
		{
#if TM_POS
			// right door
			if (ScreenIndex + 1 < ScreenCount)
			{
				TScreen.doorRight = true;
				Screens[ScreenIndex + 1].doorLeft = true;
				ScreenPos.x += 1;
			}
#else
			// left door
			if (ScreenIndex + 1 < ScreenCount)
			{
				TScreen.doorLeft = true;
				Screens[ScreenIndex + 1].doorRight = true;
				ScreenPos.x -= 1;
			}
#endif
		}
		else
		{
#if TM_POS
			// top door
			if (ScreenIndex + 1 < ScreenCount)
			{
				TScreen.doorTop = true;
				Screens[ScreenIndex + 1].doorBottom = true;
				ScreenPos.y += 1;
			}
#else
			// bottom door
			if (ScreenIndex + 1 < ScreenCount)
			{
				TScreen.doorBottom = true;
				Screens[ScreenIndex + 1].doorTop = true;
				ScreenPos.y -= 1;
			}
#endif
		}
	}

	glm::ivec2 ScreenTileDim = State.World.ScreenTileDim;
	glm::ivec2 ScreenTileDimHalf = State.World.ScreenTileDimHalf;
	for (auto& Screen : Screens)
	{
		for (i32 TileY = -ScreenTileDimHalf.y; TileY <= ScreenTileDimHalf.y; ++TileY)
		{
			for (i32 TileX = -ScreenTileDimHalf.x; TileX <= ScreenTileDimHalf.x; ++TileX)
			{
				glm::ivec2 Tile(TileX, TileY);
				glm::ivec3 TilePosition(Screen.ScreenPos * ScreenTileDim + Tile, Screen.ScreenZ);
				Engine::EObjectType TileValue = Engine::EObjectType::Empty;

				if ((TileX == -ScreenTileDimHalf.x || TileX == ScreenTileDimHalf.x) && (TileY == -ScreenTileDimHalf.y || TileY == ScreenTileDimHalf.y))
				{
					if (TileY == ScreenTileDimHalf.y)
					{
						if (TileX == ScreenTileDimHalf.x)
							TileValue = Engine::EObjectType::WallTopLeft;
						else
							TileValue = Engine::EObjectType::WallTopRight;
					}
					else
					{
						if (TileX == ScreenTileDimHalf.x)
							TileValue = Engine::EObjectType::WallBottomLeft;
						else
							TileValue = Engine::EObjectType::WallBottomRight;
					}
				}
				else
				{
					if ((TileX == -ScreenTileDimHalf.x || TileX == ScreenTileDimHalf.x))
					{
						TileValue = Engine::EObjectType::WallVerticalCenter;
						if ((Screen.doorRight && TileX == ScreenTileDimHalf.x) || (Screen.doorLeft && TileX == -ScreenTileDimHalf.x))
						{
							if (TileY == -2 || TileY == +2)
							{
								if (TileY == -2)
								{
									if (TileX == ScreenTileDimHalf.x)
										TileValue = Engine::EObjectType::WallVerticalTopRight;
									else
										TileValue = Engine::EObjectType::WallVerticalTopLeft;
								}
								else if (TileY == 2)
								{
									if (TileX == ScreenTileDimHalf.x)
										TileValue = Engine::EObjectType::WallVerticalBottomRight;
									else
										TileValue = Engine::EObjectType::WallVerticalBottomLeft;
								}
							}
							else
							{
								TileValue = Engine::EObjectType::WallVerticalCenter;
							}
						}

						if (Screen.doorRight && TileX == ScreenTileDimHalf.x && TileY >= -1 && TileY <= +1)
						{
							TileValue = Engine::EObjectType::Empty;
						}
						if (Screen.doorLeft && TileX == -ScreenTileDimHalf.x && TileY >= -1 && TileY <= +1)
						{
							TileValue = Engine::EObjectType::Empty;
						}
					}
					if (TileY == -ScreenTileDimHalf.y || TileY == ScreenTileDimHalf.y)
					{
						TileValue = Engine::EObjectType::WallHorizontalCenter;
						if ((Screen.doorTop && TileY == ScreenTileDimHalf.y) || (Screen.doorBottom && TileY == -ScreenTileDimHalf.y))
						{
							if (TileX == -2 || TileX == +2)
							{
								if (TileY == ScreenTileDimHalf.y)
								{
									if (TileX == -2)
										TileValue = Engine::EObjectType::WallHorizontalTopRight;
									else
										TileValue = Engine::EObjectType::WallHorizontalTopLeft;
								}
								else if (TileY == -ScreenTileDimHalf.y)
								{
									if (TileX == -2)
										TileValue = Engine::EObjectType::WallHorizontalBottomRight;
									else
										TileValue = Engine::EObjectType::WallHorizontalBottomLeft;
								}
							}
							else
							{
								TileValue = Engine::EObjectType::WallHorizontalCenter;
							}
						}

						if (Screen.doorBottom && TileY == -ScreenTileDimHalf.y && TileX >= -1 && TileX <= +1)
						{
							TileValue = Engine::EObjectType::Empty;
						}
						if (Screen.doorTop && TileY == ScreenTileDimHalf.y && TileX >= -1 && TileX <= +1)
						{
							TileValue = Engine::EObjectType::Empty;
						}
					}
				}

				if (TileX == -1 && TileY == -1 && Screen.stairDown)
				{
					TileValue = Engine::EObjectType::StairDown;
				}

				if (TileX == +1 && TileY == +1 && Screen.stairUp)
				{
					TileValue = Engine::EObjectType::StairUp;
				}

				if (TileValue != Engine::EObjectType::Empty)
					AddWall(State, TilePosition, glm::vec2(State.World.TileWidthInMeter), TileValue);
			}
		}
	}
}

auto SetCamera(FGameState& State)
{
	if (State.CameraTarget.entity() != entt::null && State.CameraTarget.all_of<Comp::FTransform, Comp::FWorldTransform>())
	{
		auto& Transform = State.CameraTarget.get<Comp::FTransform>();
		auto& WorldTransform = State.CameraTarget.get<Comp::FWorldTransform>();

		// Switch between smooth scrolling and fixed camera position
#if 1
		auto TargetPosDiff = glm::vec2(Engine::Subtract(State.World, WorldTransform.Position, State.CameraPosition));
		auto ScreenSpaceDiff = glm::ivec2(glm::round(TargetPosDiff / (glm::vec2(State.World.ScreenTileDim) * State.World.TileWidthInMeter)));
		if (ScreenSpaceDiff.x != 0 || ScreenSpaceDiff.y != 0 || State.CameraPosition.Chunk.z != WorldTransform.Position.Chunk.z)
		{
			auto NewCamPos = State.CameraPosition;
			auto CameraPositionOffset = glm::vec3(glm::vec2(ScreenSpaceDiff * State.World.ScreenTileDim) * State.World.TileWidthInMeter, Transform.Position.z);
			Engine::MapToChunkSpace(State.World, State.CameraPosition, CameraPositionOffset, NewCamPos);
			State.CameraPosition = NewCamPos;
		}
#else
		State.CameraPosition = WorldTransform.Position;
#endif
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

	State.BackgroundTexture.Pixels = (u32*)Memory.TempStorage;
	LoadImage(State.BackgroundTexture, "test/background.png");
	State.BgSprite = FSprite{ .Texture = &State.BackgroundTexture, .Max = { State.BackgroundTexture.Width, State.BackgroundTexture.Height } };

	State.PlayerTexture.Pixels =
		(u32*)((u8*)State.BackgroundTexture.Pixels + State.BackgroundTexture.Width * State.BackgroundTexture.Height * State.BackgroundTexture.Channels);
	LoadImage(State.PlayerTexture, "test/link_atlas.png");

	// 18x24
	State.WallTexture.Pixels = (u32*)((u8*)State.PlayerTexture.Pixels + State.PlayerTexture.Width * State.PlayerTexture.Height * State.PlayerTexture.Channels);
	LoadImage(State.WallTexture, "test/HyruleCastleHedge.png");

	State.WallSprite.Texture = &State.WallTexture;
	State.WallSprite.Min = { 0 * 32, 0 };
	State.WallSprite.Max = State.WallSprite.Min + glm::uvec2{ 32, 32 };
	State.WallSprite.Pivot = { 16, 13 };

	usize WorldArenaSize = Memory.PermanentStorageSize - sizeof(FGameState);
	void* WorldArenaPointer = (u8*)Memory.PermanentStorage + sizeof(FGameState);
	Memory::Arena::Initialize(State.WorldArena, WorldArenaSize, WorldArenaPointer);

	State.World.TileWidthInMeter = 1.5f;
	State.World.ChunkSize = glm::uvec2(16);
	State.World.TileWidthInPixels = 32;
	State.World.PixelsPerMeter = State.World.TileWidthInPixels / State.World.TileWidthInMeter;

	State.World.ScreenTileDim = glm::ivec2(21, 15);
	State.World.ScreenTileDimHalf = State.World.ScreenTileDim / 2;

	GenerateTilemap(State, 2000);

	State.PlayerFrictionCoeficient = 20.f;

	AddPlayer(State, { 0, 0, 0 });

	u32 Base = 740 * 2 + 1;
	// Player Idle
	{
		glm::uvec2 BaseOffset = { Base, 21 };
		glm::uvec2 BaseSize = { 24, 32 };
		glm::uvec2 BasePivot = glm::uvec2(12, 16);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteIdle.size(); i++)
		{
			State.PlayerSpriteIdle[i] = BaseSprite;
			State.PlayerSpriteIdle[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteIdle[i].Max = State.PlayerSpriteIdle[i].Min + BaseSize + glm::uvec2(-1, 0);
		}
	}

	// Player move down
	{
		glm::uvec2 BaseOffset = { 324 + Base, 21 };
		glm::uvec2 BaseSize = { 23, 34 };
		glm::uvec2 BasePivot = BaseSize / glm::uvec2(2);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteMoveDown.size(); i++)
		{
			State.PlayerSpriteMoveDown[i] = BaseSprite;
			State.PlayerSpriteMoveDown[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteMoveDown[i].Max = State.PlayerSpriteMoveDown[i].Min + BaseSize + glm::uvec2(-1, 0);
		}
	}

	// Player move right
	{
		glm::uvec2 BaseOffset = { 324 + Base, 21 + 1 + 35 };
		glm::uvec2 BaseSize = { 24, 32 };
		glm::uvec2 BasePivot = BaseSize / glm::uvec2(2);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteMoveRight.size(); i++)
		{
			State.PlayerSpriteMoveRight[i] = BaseSprite;
			State.PlayerSpriteMoveRight[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteMoveRight[i].Max = State.PlayerSpriteMoveRight[i].Min + BaseSize + glm::uvec2(-1, 0);
		}
	}

	// Player move left
	{
		glm::uvec2 BaseOffset = { 324 + Base, 21 + 1 + 35 };
		glm::uvec2 BaseSize = { 24, 32 };
		glm::uvec2 BasePivot = BaseSize / glm::uvec2(2);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteMoveLeft.size(); i++)
		{
			State.PlayerSpriteMoveLeft[i] = BaseSprite;
			State.PlayerSpriteMoveLeft[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteMoveLeft[i].Max = State.PlayerSpriteMoveLeft[i].Min + BaseSize;
			State.PlayerSpriteMoveLeft[i].Min.x += BaseSize.x - 1;
			State.PlayerSpriteMoveLeft[i].Max.x -= BaseSize.x - 1;
		}
	}

	// Player move up
	{
		glm::uvec2 BaseOffset = { 324 + Base, 21 + 1 + 35 + 1 + 33 };
		glm::uvec2 BaseSize = { 26, 35 };
		glm::uvec2 BasePivot = BaseSize / glm::uvec2(2);
		auto BaseSprite = FSprite{ .Texture = &State.PlayerTexture, .Pivot = BasePivot };
		for (u32 i = 0; i < State.PlayerSpriteMoveUp.size(); i++)
		{
			State.PlayerSpriteMoveUp[i] = BaseSprite;
			State.PlayerSpriteMoveUp[i].Min = BaseOffset + (BaseSize + glm::uvec2{ 1, 0 }) * glm::uvec2(i, 0);
			State.PlayerSpriteMoveUp[i].Max = State.PlayerSpriteMoveUp[i].Min + BaseSize + glm::uvec2(-1, 0);
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
	auto SpriteYMin = Sprite.Min.y;
	auto SpriteYMax = Sprite.Max.y;
	if (Sprite.Min.y > Sprite.Max.y)
	{
		SpriteYMin = Sprite.Max.y;
		SpriteYMax = Sprite.Min.y;
	}

	glm::ivec2 Min = glm::round(InMin);
	glm::ivec2 Max = glm::round(InMax);

	Min -= Sprite.Pivot;
	Max -= Sprite.Pivot;

	auto OffsetSource = Min;

	Min = glm::clamp(Min, glm::ivec2(0), glm::ivec2(Buffer.Width - 1, Buffer.Height - 1));
	Max = glm::clamp(Max, glm::ivec2(0), glm::ivec2(Buffer.Width - 1, Buffer.Height - 1));

	OffsetSource = Min - OffsetSource;

	u32 Pitch = Buffer.Width * Buffer.BytesPerPixel;
	u8* Row = Buffer.Backbuffer + Min.x * Buffer.BytesPerPixel + Min.y * Pitch;
	u32 BufferX = Min.x;
	u32 BufferY = Min.y;

	for (u32 Y = SpriteYMin + OffsetSource.y; Y <= SpriteYMax && Y < Sprite.Texture->Height && BufferY <= Max.y; ++Y)
	{
		u32* Pixel = (u32*)Row;
		BufferX = Min.x;
		for (u32 X = SpriteXMin + OffsetSource.x; X <= SpriteXMax && X < Sprite.Texture->Width && BufferX <= Max.x; ++X)
		{
			u32 Color = Sprite.Texture->Pixels[Y * Sprite.Texture->Width + X];

			if (Sprite.Min.x > Sprite.Max.x)
			{
				auto offsetx = Sprite.Max.x + (Sprite.Min.x - X);
				Color = Sprite.Texture->Pixels[Y * Sprite.Texture->Width + offsetx];
			}
			else if (Sprite.Min.y > Sprite.Max.y)
			{
				auto offsety = Sprite.Max.y + (Sprite.Min.y - Y);
				Color = Sprite.Texture->Pixels[offsety * Sprite.Texture->Width + X];
			}
			else if (Sprite.Min.x > Sprite.Max.x && Sprite.Min.y > Sprite.Max.y)
			{
				auto offsetx = Sprite.Max.x + (Sprite.Min.x - X);
				auto offsety = Sprite.Max.y + (Sprite.Min.y - Y);
				Color = Sprite.Texture->Pixels[offsety * Sprite.Texture->Width + offsetx];
			}

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
	auto MoveView = State.Registry.view<Comp::FTransform, Comp::FMovement, Comp::FCollider>();
	auto ColView = State.Registry.view<Comp::FTransform, Comp::FCollider, Comp::FActiveTag>();

	// Calculate new player position
	for (auto [Entity, Transform, Movement, Collider] : MoveView.each())
	{
		// TODO(Rafis): TEMP, this should handle animation system
		if (State.Registry.any_of<Comp::FPlayer>(Entity))
		{
			State.PlayerSpriteMoveIndex += glm::length(Movement.Velocity) * DeltaTime;
		}

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
			for (auto [ColEntity, ColTransform, ColCollider, ColActiveTag] : ColView.each())
			{
				if (Entity != ColEntity)
				{
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
					Transform.Position.z += TeleportComp.Z;
					break;
				}
			}
			else
			{
				break;
			}
		}
	}
}

RE_API auto GameUpdateAndRender(FGameMemory& Memory, f32 DeltaTime, FOffscreenBuffer& Buffer, const FInputGamepad& Gamepad) -> void
{
	FGameState& State = *(FGameState*)Memory.PermanentStorage;

	State.World.DrawPosition = glm::vec2(0.5f * Buffer.Width, 0.5f * Buffer.Height);

	SetCamera(State);

	// Begin simulation
	auto FrustrumBounds = Math::RectFromLeftTop(glm::ivec3(State.CameraPosition.Chunk) + glm::ivec3(-5, -5, 0), glm::ivec3(11, 11, 1));
	State.ActiveRegion = Engine::BeginSimRegion(State.Registry, State.World, State.CameraPosition, FrustrumBounds);

	static bool bOptionPressed = false;
	static bool bOptionWasPressed = false;
	bOptionWasPressed = bOptionPressed && Gamepad.GamepadOption.bPressed;
	bOptionPressed = Gamepad.GamepadOption.bPressed;
	if (bOptionPressed && !bOptionWasPressed)
	{
		// State.PlayerPosition.Chunk = glm::ivec3(9, 3, State.PlayerPosition.Chunk.z);
		// State.PlayerPosition.Local = glm::vec2(0.0f, 0.0f);
		// State.PlayerAcceleration = (State.PlayerAcceleration == 4.0f) ? 8.0f : 4.0f;
		// State.PlayerAcceleration += 1.f;
	}

	State.PlayerSpriteIdleIndex += 8.0f * DeltaTime;

	PlayerMove(State, Gamepad, DeltaTime);

	auto MoveView = State.Registry.view<Comp::FMovement, Comp::FDrawSprite, Comp::FActiveTag>();
	for (auto [entity, Movement, Draw, ActiveTag] : MoveView.each())
	{
		u32 index = 0;
		if (glm::abs(Movement.Velocity.x) < 0.1f && glm::abs(Movement.Velocity.y) < 0.1f)
		{
			index = (u32)(glm::round(State.PlayerSpriteIdleIndex)) % State.PlayerSpriteIdle.size();
			Draw.Sprite = State.PlayerSpriteIdle[index];
		}
		else
		{
			if (glm::abs(Movement.Velocity.x) > glm::abs(Movement.Velocity.y))
			{
				if (Movement.Velocity.x > 0)
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveRight.size();
					Draw.Sprite = State.PlayerSpriteMoveRight[index];
				}
				else
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveLeft.size();
					Draw.Sprite = State.PlayerSpriteMoveLeft[index];
				}
			}
			else
			{
				if (Movement.Velocity.y > 0)
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveUp.size();
					Draw.Sprite = State.PlayerSpriteMoveUp[index];
				}
				else
				{
					index = (u32)(glm::round(State.PlayerSpriteMoveIndex)) % State.PlayerSpriteMoveDown.size();
					Draw.Sprite = State.PlayerSpriteMoveDown[index];
				}
			}
		}
	}

	// Clear screen
	DrawRect(Buffer, glm::vec2{ 0.0f, 0.0f }, glm::vec2{ Buffer.Width, Buffer.Height }, glm::vec3{ 0.08f, 0.17f, 0.85f });

	DrawImage(Buffer, glm::vec2{ 0, 0 }, glm::vec2{ State.BgSprite.Texture->Width, State.BgSprite.Texture->Height }, State.BgSprite);

	// Collision draw
#if defined(RE_INTERNAL) && 0
	auto DrawDebugView = State.Registry.view<Comp::FTransform, Comp::FCollider, Comp::FActiveTag>();
	for (auto [entity, Transform, Collider, ActiveTag] : DrawDebugView.each())
	{
		auto ColDrawSize = Collider.Size * State.World.PixelsPerMeter;
		auto DrawPosition = State.World.DrawPosition + glm::vec2(Transform.Position) * State.World.PixelsPerMeter * glm::vec2(1.0f, -1.0f);
		DrawRect(Buffer, DrawPosition - ColDrawSize * glm::vec2(0.5f), DrawPosition + ColDrawSize * glm::vec2(0.5f), glm::vec3(0.25f, 0.65f, 0.1f));
	}
#endif

	// main draw
	auto DrawView = State.Registry.view<Comp::FTransform, Comp::FSize, Comp::FDrawSprite, Comp::FActiveTag>();
	for (auto [entity, Transform, Size, Draw, ActiveTag] : DrawView.each())
	{
		auto DrawPosition = State.World.DrawPosition + glm::vec2(Transform.Position) * State.World.PixelsPerMeter * glm::vec2(1.0f, -1.0f);
		DrawImage(Buffer, DrawPosition, DrawPosition + glm::vec2(Draw.Sprite.Max - Draw.Sprite.Min), Draw.Sprite);
	}

	// Center of the screen
	{
		DrawRect(Buffer, State.World.DrawPosition - glm::vec2(4.f), State.World.DrawPosition + glm::vec2(4.f), glm::vec3(0.15f, 0.08f, 0.10f));
	}

	Engine::EndSimRegion(State.ActiveRegion);
}
