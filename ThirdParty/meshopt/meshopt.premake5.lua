function linkMeshopt()
	links {
		"meshopt"
	}

	externalincludedirs {
		"%{THIRD_PARTY_DIR}/meshopt/meshopt/src",
	}
end

project "meshopt"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"

	includedirs {
		"meshopt/src",
	}

	files {
		"meshopt/src/**.*",
	}