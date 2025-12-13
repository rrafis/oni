#pragma once

#define IS_PLATFORM(Platform) RE_PLATFORM == Platform

#if IS_PLATFORM(Mac)
#	include "Mac/MacPlatformTypes.h"
#elif IS_PLATFORM(Windows)
#	include "Windows/WindowsPlatformTypes.h"
#endif
