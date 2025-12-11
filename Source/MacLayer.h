#pragma once

#include <sys/types.h>

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

struct FMacSurfaceDesc
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

struct FMacInternalState
{
	u8* Backbuffer;
	u32 Width;
	u32 Height;
	u8 BytesPerPixel;

	f32 DeltaTime;

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

auto MacRunLoop(const FMacInternalState& InternalState) -> void
{
	static f32 xOffset = 0;
	static f32 yOffset = 0;
	static f32 OffsetSpeed = 300.0f;

	u32 Width = InternalState.Width;
	u32 Height = InternalState.Height;
	u32 Pitch = Width * InternalState.BytesPerPixel;
	u8* Row = InternalState.Backbuffer;

	xOffset += InternalState.GamepadThumbstickLeft.X * OffsetSpeed * InternalState.DeltaTime;
	yOffset += InternalState.GamepadThumbstickLeft.Y * OffsetSpeed * InternalState.DeltaTime;

	for (u32 Y = 0; Y < Height; ++Y)
	{
		u32* Pixel = (u32*)Row;

		for (u32 X = 0; X < Width; ++X)
		{
			u8 C1 = X + xOffset;
			u8 C2 = Y + yOffset;

			*Pixel++ = ((C2 << 16) | C1);
		}

		Row += Pitch;
	}
}

auto MacGetSurfaceDesc() -> FMacSurfaceDesc
{
	FMacSurfaceDesc Props;
	Props.Width = 1280;
	Props.Height = 5 * Props.Width / 8;
	Props.bCenterd = true;
	Props.Title = "Testbed Window";

	return Props;
}
