workspace "RE"
	staticruntime "Off"

	targetdir "Binaries/%{cfg.platform}/%{cfg.buildcfg}"
	objdir "Intermediate"

	configurations {
		"Debug",
		"Shipping",
	}

	platforms {
		"Windows",
		"Mac",
	}

	flags {
		"MultiProcessorCompile",
	}

	filter "platforms:Windows"
		system "windows"
		architecture "x64"

		defines {
			"RE_PLATFORM=Windows",
		}
	filter {}

	filter "platforms:Mac"
		system "macosx"
		architecture "arm64"
		defines {
			"RE_PLATFORM=Mac",
		}
	filter {}

	filter "configurations:Debug"
		symbols "On"
		runtime "Debug"

		defines {
			"RE_DEBUG",
			"RE_ENABLE_ASSERT",
			"RE_INTERNAL"
		}
	filter {}

	filter "configurations:Shipping"
		symbols "Full"
		runtime "Release"

		defines {
			"RE_SHIPPING",
		}
	filter {}

THIRD_PARTY_DIR = "%{wks.location}/ThirdParty"

include "ThirdParty/imgui/imgui.premake5.lua"
include "ThirdParty/cereal/cereal.premake5.lua"
include "ThirdParty/cgltf/cgltf.premake5.lua"
include "ThirdParty/Jolt/Jolt.premake5.lua"

include "Engine/premake5.lua"
include "Testbed/premake5.lua"
include "Oni/premake5.lua"