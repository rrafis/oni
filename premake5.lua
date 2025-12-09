workspace "RE"
	staticruntime "Off"

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
		}
	filter {}

	filter "configurations:Shipping"
		symbols "Full"
		runtime "Release"

		defines {
			"RE_SHIPPING",
		}
	filter {}

project "Testbed"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"

	targetdir "Binaries/%{cfg.platform}/%{cfg.buildcfg}"
	objdir "Intermediate"

	includedirs {
		"Source",
	}

	externalincludedirs {
		"ThirdParty/entt/src",
		"ThirdParty/glm",
	}

	files {
		"Source/**.*",
	}

	filter "platforms:Mac"
		externalincludedirs {
			"ThirdParty/metal-cpp",
		}
	filter {}

