#pragma once

#include "PlatformTypes.h"

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

	f32 TargetFPS = 120.0f;

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
