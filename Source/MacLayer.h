#pragma once

#include <sys/types.h>

#include <glm/glm.hpp>

// Unsigned int types.

/** @brief Unsigned 8-bit integer */
using u8 = u_int8_t;
/** @brief Unsigned 16-bit integer */
using u16 = u_int16_t;
/** @brief Unsigned 32-bit integer */
using u32 = u_int32_t;
/** @brief Unsigned 64-bit integer */
using u64 = u_int64_t;

// Signed int types.

/** @brief Signed 8-bit integer */
using i8 = int8_t;
/** @brief Signed 16-bit integer */
using i16 = int16_t;
/** @brief Signed 32-bit integer */
using i32 = int32_t;
/** @brief Signed 64-bit integer */
using i64 = int64_t;

// Floating point types

/** @brief 32-bit floating point number */
using f32 = float;
/** @brief 64-bit floating point number */
using f64 = double;

/** @brief Size */
using usize = size_t;

static_assert(sizeof(u8) == 1);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(u64) == 8);

static_assert(sizeof(i8) == 1);
static_assert(sizeof(i16) == 2);
static_assert(sizeof(i32) == 4);
static_assert(sizeof(i64) == 8);

static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

constexpr auto KiB(u64 Amount) -> u64
{
	return Amount * 1024;
}

constexpr auto MiB(u64 Amount) -> u64
{
	return KiB(Amount) * 1024;
}

constexpr auto GiB(u64 Amount) -> u64
{
	return MiB(Amount) * 1024;
}

struct FGameSurfaceDesc
{
	u32 XPos = 0, YPos = 0;
	u32 Width = 1280, Height = 720;

	u32 MinWidth = 1280, MinHeight = 720;
	u32 MaxWidth = 0xFFFFFFFF, MaxHeight = 0xFFFFFFFF;

	bool bCenterd = true;
	bool bResizable = true;
	bool bMovable = true;
	bool bClosable = true;
	bool bMinimizable = true;
	bool bMaximizable = true;
	bool bSupportFullscreen = true;

	u32 BackgroundColor = 0xFFFFFFFF;
	bool bTransparent = false;
	bool bUseFrame = true;
	bool bHasShadow = true;

	bool bVisible = true;
	bool bStartFullscreen = false;
	bool bModal = false;

	const char* Title = "RE Engine";
	const char* Name = "REEngineMain";
	const char* IconPath = "";
};

struct FInputButtonState
{
	bool bPressed;
	f32 Value; // [0, 1]
};

struct FInputAxisState
{
	f32 X; // [-1, 1], 0 means inside deadzone
	f32 Y; // [-1, 1], 0 means inside deadzone
};

struct FInputGamepad
{
	FInputButtonState GamepadA;
	FInputButtonState GamepadB;
	FInputButtonState GamepadX;
	FInputButtonState GamepadY;

	FInputButtonState GamepadDpadUp;
	FInputButtonState GamepadDpadDown;
	FInputButtonState GamepadDpadLeft;
	FInputButtonState GamepadDpadRight;

	FInputButtonState GamepadShoulderLeft;
	FInputButtonState GamepadShoulderRight;
	FInputButtonState GamepadThumbstickButtonLeft;
	FInputButtonState GamepadThumbstickButtonRight;

	FInputButtonState GamepadStart;
	FInputButtonState GamepadOption;

	FInputButtonState GamepadTriggerLeft;
	FInputButtonState GamepadTriggerRight;

	FInputAxisState GamepadDpadAxis;
	FInputAxisState GamepadThumbstickLeft;
	FInputAxisState GamepadThumbstickRight;
};

struct FOffscreenBuffer
{
	u8* Backbuffer;
	u32 Width;
	u32 Height;
	u8 BytesPerPixel;
};

struct FGameMemory
{
	u64 PermanentStorageSize;
	void* PermanentStorage;

	u64 TempStorageSize;
	void* TempStorage;
};

auto GameGetSurfaceDesc() -> FGameSurfaceDesc
{
	FSurfaceDesc Props;
	Props.Width = 1280;
	Props.Height = 5 * Props.Width / 8;
	Props.bCenterd = true;
	Props.Title = "Testbed Window";

	return Props;
}

struct FGameState
{
	f32 xOffset;
	f32 yOffset;
	f32 OffsetSpeed;
};

auto GameInitialize(FGameMemory& Memory) -> void
{
	// TODO(Rafis): Assert Memory is valid

	FGameState& State = *(FGameState*)Memory.PermanentStorage;
	State.xOffset = 0;
	State.yOffset = 0;
	State.OffsetSpeed = 2 * 255.f;
}

auto GameUpdateAndRender(FGameMemory& Memory, f32 DeltaTime, FOffscreenBuffer& Buffer, const FInputGamepad& Gamepad) -> void
{
	FGameState& State = *(FGameState*)Memory.PermanentStorage;

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
