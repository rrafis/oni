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

	filter "toolset:clang"
		buildoptions { "-Werror=return-type" }
		-- buildoptions { "-Wall", "-Wextra", "-Werror=return-type" }
	filter {}

THIRD_PARTY_DIR = "%{wks.location}/ThirdParty"

function linkDynamic(location, libs, extraLibs)
	libdirs {
		"%{cfg.targetdir}"
	}

	for _, lib in ipairs(libs) do
		links {
			lib,
		}

		filter "platforms:Mac"
			prebuildcommands {
				"{COPY} " .. location .. "/Binaries/%{cfg.platform}/%{cfg.buildcfg}/lib" .. lib .. ".dylib %{cfg.targetdir}",
			}
		filter {}
	end

	if extraLibs then
		for _, lib in ipairs(extraLibs) do
			filter "platforms:Mac"
				prebuildcommands {
					"{COPY} " .. location .. "/Binaries/%{cfg.platform}/%{cfg.buildcfg}/lib" .. lib .. ".dylib %{cfg.targetdir}",
				}
			filter {}
		end
	end
end

include "ThirdParty/assimp/assimp.premake5.lua"
include "ThirdParty/imgui/imgui.premake5.lua"
include "ThirdParty/meshopt/meshopt.premake5.lua"
include "ThirdParty/cereal/cereal.premake5.lua"
include "ThirdParty/cgltf/cgltf.premake5.lua"
include "ThirdParty/Jolt/Jolt.premake5.lua"

include "Engine/premake5.lua"
include "Testbed/premake5.lua"
include "Oni/premake5.lua"