project "ReEngine"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"

	includedirs {
		"Source",
	}

	externalincludedirs {
		"%{wks.location}/ThirdParty/entt/src",
		"%{wks.location}/ThirdParty/glm",
	}

	files {
		"Source/**.*",
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
			"MetalKit.framework",
			"QuartzCore.framework",
			"GameController.framework",
		}
	filter {}