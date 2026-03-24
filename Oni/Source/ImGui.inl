#include "Platform.h"

#include <backends/imgui_impl_metal.h>
#include <backends/imgui_impl_osx.h>
#include <imgui.h>

#include <Metal/Metal.hpp>

namespace ImGui::Mac
{
	auto Init(MTL::Device* Device, void* MetalView)
	{
		// Setup Dear ImGui context
		// FIXME: This example doesn't have proper cleanup...
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Renderer backend
		ImGui_ImplOSX_Init(MetalView);
		ImGui_ImplMetal_Init(Device);
	}

	auto Shutdown() -> void
	{
		ImGui_ImplMetal_Shutdown();
		ImGui_ImplOSX_Shutdown();
		ImGui::DestroyContext();
	}

	auto BeginFrame(MTL4::RenderPassDescriptor* MetalPass, void* MetalView, f32 Width, f32 Height) -> void
	{
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize.x = Width;
		io.DisplaySize.y = Height;

		// CGFloat framebufferScale = view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

		// Start the Dear ImGui frame
		ImGui_ImplMetal_NewFrame(MetalPass);
#if TARGET_OS_OSX
		ImGui_ImplOSX_NewFrame(MetalView);
#endif
		ImGui::NewFrame();
	}

	auto EndFrame(MTL4::CommandBuffer* MetalCommandBuffer, MTL4::RenderCommandEncoder* MetalEncoder, MTL4::CommitOptions* Options) -> void
	{
		ImGui::Render();
		ImDrawData* draw_data = ImGui::GetDrawData();

		MetalEncoder->pushDebugGroup(NS::String::string("Dear ImGui rendering", NS::UTF8StringEncoding));
		ImGui_ImplMetal_RenderDrawData(draw_data, MetalCommandBuffer, MetalEncoder, Options);
		MetalEncoder->popDebugGroup();
	}
} // namespace Utils
