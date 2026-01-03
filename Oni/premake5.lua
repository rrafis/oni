project "Oni"
	kind "SharedLib"
	language "C++"
	cppdialect "C++20"

	includedirs {
		"Source",
	}

	defines {
		"RE_EXPORT",
	}

	externalincludedirs {
		"%{wks.location}/ThirdParty/entt/src",
		"%{wks.location}/ThirdParty/glm",
		"%{wks.location}/ThirdParty/stb",
		"%{wks.location}/Engine/Source",
	}

	files {
		"Source/Oni.cpp",
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
		}
	filter {}

