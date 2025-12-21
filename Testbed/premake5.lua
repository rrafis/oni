project "Testbed"
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
		"Source/Game.cpp",
	}
