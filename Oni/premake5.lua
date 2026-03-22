project "Oni"
	kind "SharedLib"
	language "C++"
	cppdialect "C++20"

	includedirs {
		"Source",
	}

	defines {
		"RE_EXPORT",
		"JPH_CROSS_PLATFORM_DETERMINISTIC",
	}

	filter "configurations:Debug"
		defines {
			"JPH_ENABLE_ASSERTS",
			"TRACK_BROADPHASE_STATS",
			"TRACK_NARROWPHASE_STATS",
			"JPH_TRACK_SIMULATION_STATS",
			"JPH_TRACK_SIMULATION_STATS",
		}
	filter {}

	linkCgltf()

	externalincludedirs {
		"%{wks.location}/ThirdParty/entt/src",
		"%{wks.location}/ThirdParty/glm",
		"%{wks.location}/ThirdParty/stb",
		"%{wks.location}/ThirdParty/Jolt/JoltPhysics",
		"%{wks.location}/Engine/Source",
	}

	files {
		"Source/**.*",
		"../Content/**.*"
	}

	links {
		"Jolt",
	}

	filter "platforms:Mac"
		externalincludedirs {
			"%{wks.location}/ThirdParty/metal-cpp",
		}

		links {
			"AppKit.framework",
			"Foundation.framework",
			"Metal.framework",
			"MetalFX.framework",
			"QuartzCore.framework",
			"ModelIO.framework",
			"MetalKit.framework",
		}

		filter { "action:xcode*" }
			buildoptions { "-D_RE_XCODE" }
		filter {}
	filter {}

