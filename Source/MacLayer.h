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

struct FMacInternalState
{
	u8* Backbuffer;
	u32 Width;
	u32 Height;
	u8 BytesPerPixel;

	f32 DeltaTime;
};

auto MacRunLoop(const FMacInternalState& InternalState) -> void
{
	for (u32 y = 0; y < InternalState.Height; ++y)
	{
		for (u32 x = 0; x < InternalState.Width; ++x)
		{
			bool checker = ((x / 32) ^ (y / 32)) & 1;
			u8* p = InternalState.Backbuffer + (y * InternalState.Width * InternalState.BytesPerPixel) + x * InternalState.BytesPerPixel;
			p[0] = checker ? 0xFF - 0x15 : 0x15; // B
			p[1] = checker ? 0xFF - 0x15 : 0x15; // G
			p[2] = checker ? 0xFF - 0x15 : 0x15; // R
			p[3] = 0xFF;						 // A
		}
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
