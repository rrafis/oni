#include <Platform.h>

#import <AppKit/AppKit.h>
// #import <Cocoa/Cocoa.h>

#include <mach/mach_time.h>
#include <unistd.h>

#include <filesystem>

namespace Platform
{
	namespace fs = std::filesystem;
	auto GetResourcesPath() -> const fs::path&
	{
		static fs::path cachedPath;
		static bool bCached = false;
		if (!bCached)
		{
			@autoreleasepool
			{
				NSString *bundlePath = NSBundle.mainBundle.bundlePath;
				NSString *resourcePath;
				if ([bundlePath.pathExtension isEqualToString:@"app"])
				{
					resourcePath = [NSBundle.mainBundle resourcePath];
					cachedPath = fs::weakly_canonical(fs::path([resourcePath UTF8String]) / "Content");
				} else
				{
					resourcePath = [[NSBundle.mainBundle.bundlePath stringByDeletingLastPathComponent] stringByStandardizingPath];
					cachedPath = fs::weakly_canonical(fs::path([resourcePath UTF8String]) / ".." / ".." / "Content");
				}
			}
		}
		return cachedPath;
	}

	auto Sleep(u64 Ms) -> void
	{
	#if _POSIX_C_SOURCE >= 199309L
		struct timespec ts;
		ts.tv_sec = Ms / 1000;
		ts.tv_nsec = (Ms % 1000) * 1000 * 1000;
		nanosleep(&ts, 0);
	#else
		if (Ms >= 1000)
		{
			sleep(Ms / 1000);
		}
		usleep((Ms % 1000) * 1000);
	#endif
	}

	auto GetAbsoluteTime() -> f64
	{
		mach_timebase_info_data_t ClockTimebase;
		mach_timebase_info(&ClockTimebase);

		u64 MachAbsolute = mach_absolute_time();

		u64 nanos = (f64)(MachAbsolute * (u64)ClockTimebase.numer) / (f64)ClockTimebase.denom;
		return nanos / 1.0e9; // Convert to seconds
	}
}