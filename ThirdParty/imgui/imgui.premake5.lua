function linkImGui()
	links {
		"ImGui"
	}

	externalincludedirs {
		"%{THIRD_PARTY_DIR}/imgui/imgui",
		"%{THIRD_PARTY_DIR}/imgui/imgui/backends",
		"%{THIRD_PARTY_DIR}/metal-cpp",
	}

	-- Required for gmake on macOS
	filter "platforms:Mac"
		defines {
			"IMGUI_IMPL_METAL_CPP",
			"IMGUI_IMPL_METAL_CPP_EXTENSIONS",
		}

		externalincludedirs {
			"%{THIRD_PARTY_DIR}/metal-cpp",
		}

		links {
			"AppKit.framework",
			"GameController.framework",
			"Metal.framework",
			"Foundation.framework",
			"MetalFX.framework",
			"MetalKit.framework",
			"QuartzCore.framework",
		}
	filter {}
end

project "ImGui"
	kind "SharedLib"
	language "C++"
	cppdialect "C++20"

	includedirs {
		"imgui",
		"imgui/backends",
	}

	externalincludedirs {
	}

	files {
		"imgui/imconfig.h",
		"imgui/imgui_demo.cpp",
		"imgui/imgui_draw.cpp",
		"imgui/imgui_internal.h",
		"imgui/imgui_tables.cpp",
		"imgui/imgui_widgets.cpp",
		"imgui/imgui.cpp",
		"imgui/imgui.h",
		"imgui/imstb_rectpack.h",
		"imgui/imstb_textedit.h",
		"imgui/imstb_truetype.h",
		"imgui/misc/cpp/imgui_stdlib.h",
		"imgui/misc/cpp/imgui_stdlib.cpp",
	}

	filter "platforms:Mac"
		files {
			"imgui/backends/imgui_impl_metal.h",
			"imgui/backends/imgui_impl_metal.mm",
			"imgui/backends/imgui_impl_osx.h",
			"imgui/backends/imgui_impl_osx.mm",
		}

		defines {
			"IMGUI_IMPL_METAL_CPP",
			"IMGUI_IMPL_METAL_CPP_EXTENSIONS",
		}

		externalincludedirs {
			"%{THIRD_PARTY_DIR}/metal-cpp",
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
	filter{}