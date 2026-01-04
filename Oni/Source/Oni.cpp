#include <Platform.h>

#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "MeshUtils.h"
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <MetalFX/MetalFX.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <filesystem>

constexpr f32 s_UnitsInMeter = 30.f;

struct FMesh
{
	Engine::Mesh::FMeshRenderer Render;
	glm::vec3 Position;
};

struct FGameState
{
	MTL::Device* Device;
	MTL::CommandQueue* Queue;
	MTL::RenderPassDescriptor* DrawableRenderPass;
	MTL::RenderPipelineState* Pipeline;
	MTL::PixelFormat DepthFormat;
	MTL::Texture* DepthTexture;
	MTL::DepthStencilState* DepthStencilState;

	NS::SharedPtr<MTL::SharedEvent> PacingEvent;
	u64 PacingTimeStampIndex;
	u64 MaxFramesInFlight;
	u64 Frame;

	std::vector<FMesh> Meshes;

	glm::vec3 CameraPosition;

	glm::mat4 View;
	glm::mat4 Projection;

	glm::mat4 MVP;

	u32 Width;
	u32 Height;
};

constexpr auto ContentPath = "Content";

auto LoadMesh(MTL::Device* Device, const char* Path) -> Engine::Mesh::FMeshRenderer
{
	auto LoadPath = std::filesystem::current_path() / ContentPath / Path;
	auto SwordMesh = Engine::Mesh::MDLIO::ExportMeshFromPath(Device, LoadPath.c_str());

	return Engine::Mesh::Metal::ProcessMeshData(Device, SwordMesh);
}

static FGameSurfaceDesc Props;
RE_API auto GameGetSurfaceDesc() -> FGameSurfaceDesc*
{
	Props.Width = 1260.f;
	Props.Height = 900.f;
	Props.bCenterd = true;
	Props.Title = "Testbed Window";

	return &Props;
}

RE_API auto GameInitialize(FGameMemory& Memory) -> void
{
	// TODO(Rafis): Assert Memory is valid

	// Map game state to the memory
	FGameState& State = *(FGameState*)Memory.PermanentStorage;
	// Construct game state in place
	new (&State) FGameState();

	State.Width = Props.Width;
	State.Height = Props.Height;

	State.Device = MTL::CreateSystemDefaultDevice();
	State.Queue = State.Device->newCommandQueue();
	State.DrawableRenderPass = MTL::RenderPassDescriptor::alloc()->init();

	// Depth
	{
		State.DepthFormat = MTL::PixelFormatDepth32Float;

		MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::texture2DDescriptor(State.DepthFormat, State.Width, State.Height, false);
		descriptor->setUsage(MTL::TextureUsageRenderTarget);
		State.DepthTexture = State.Device->newTexture(descriptor);

		auto* DepthAttachment = State.DrawableRenderPass->depthAttachment();
		DepthAttachment->setTexture(State.DepthTexture);
		DepthAttachment->setLoadAction(MTL::LoadActionClear);
		DepthAttachment->setStoreAction(MTL::StoreActionStore);
		DepthAttachment->setClearDepth(1.0f);

		MTL::DepthStencilDescriptor* depthDesc = MTL::DepthStencilDescriptor::alloc()->init();
		depthDesc->setDepthCompareFunction(MTL::CompareFunctionLess);
		depthDesc->setDepthWriteEnabled(true);
		State.DepthStencilState = State.Device->newDepthStencilState(depthDesc);
	}

	State.PacingEvent = NS::TransferPtr(State.Device->newSharedEvent());
	State.PacingTimeStampIndex = 0;
	State.MaxFramesInFlight = 3;

	State.CameraPosition = glm::vec3(0.0f, 0.0f, 0.0f);

	State.Meshes.emplace_back();
	State.Meshes[0].Position = glm::vec3(0.0f, -0.5f, 2.0f);
	State.Meshes[0].Render = LoadMesh(State.Device, "meshes/samanosuke_future/Future Samanosuke.obj");
	State.Meshes.emplace_back();
	State.Meshes[1].Position = glm::vec3(0.0f, -0.5f, 2.0f);
	State.Meshes[1].Render = LoadMesh(State.Device, "meshes/samanosuke_weapons/Tensou lvl 3.obj");

	State.Projection = glm::perspectiveFov(glm::radians(90.0f), (f32)Props.Width, (f32)Props.Height, 0.01f * s_UnitsInMeter, 100.0f * s_UnitsInMeter);
	State.View = glm::lookAtLH(State.CameraPosition * s_UnitsInMeter, State.CameraPosition * s_UnitsInMeter + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));

	{
		const char* triShaderSource = "#include <metal_stdlib>\n"
									  "using namespace metal;\n"
									  "struct VertexIn {\n"
									  "    float3 position [[attribute(0)]];\n"
									  "    float3 normal [[attribute(1)]];\n"
									  "    float2 uv [[attribute(2)]];\n"
									  "};\n"
									  "struct VSOut {\n"
									  "	   float4 position [[position]];\n"
									  "	   float3 normal;\n"
									  "	   float2 uv;\n"
									  "};\n"
									  "vertex VSOut vertex_main(const VertexIn in [[stage_in]], constant float4x4 &MVP [[buffer(10)]]) {\n"
									  "	   VSOut out;\n"
									  "	   out.position = MVP * float4(in.position, 1.0);\n"
									  "	   out.normal = in.normal;\n"
									  "	   out.uv = in.uv;\n"
									  "	   return out;\n"
									  "}\n"
									  "fragment float4 fragment_main(VSOut in [[stage_in]]) {\n"
									  "	   return float4(in.uv.x, 0.18, in.uv.y, 1.0);\n"
									  "}";

		NS::String* shaderSource = NS::String::alloc()->init(triShaderSource, NS::StringEncoding::UTF8StringEncoding);
		MTL::CompileOptions* options = nullptr;
		NS::Error* error = nullptr;
		auto Shader = State.Device->newLibrary(shaderSource, options, &error);

		auto vertFunc = Shader->newFunction(NS::String::alloc()->init("vertex_main", NS::StringEncoding::UTF8StringEncoding));
		auto fragFunc = Shader->newFunction(NS::String::alloc()->init("fragment_main", NS::StringEncoding::UTF8StringEncoding));

		MTL::RenderPipelineDescriptor* pipeDesc = MTL::RenderPipelineDescriptor::alloc()->init();
		pipeDesc->setVertexFunction(vertFunc);
		pipeDesc->setFragmentFunction(fragFunc);
		pipeDesc->setVertexDescriptor(State.Meshes[0].Render.VertexDescriptor);
		pipeDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);
		pipeDesc->setDepthAttachmentPixelFormat(State.DepthFormat);
		State.Pipeline = State.Device->newRenderPipelineState(pipeDesc, &error);
	}
}

RE_API auto GameUpdateAndRender(FGameMemory& Memory, f32 DeltaTime, FOffscreenBuffer& Buffer, const FInputGamepad& Gamepad) -> void
{
	NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

	FGameState& State = *(FGameState*)Memory.PermanentStorage;

	++State.PacingTimeStampIndex;
	State.Frame = (State.Frame + 1) % State.MaxFramesInFlight;

	if (State.PacingTimeStampIndex > State.MaxFramesInFlight)
	{
		uint64_t const timeStampToWait = State.PacingTimeStampIndex - State.MaxFramesInFlight;
		State.PacingEvent->waitUntilSignaledValue(timeStampToWait, DISPATCH_TIME_FOREVER);
	}

	CA::MetalDrawable* Drawable = (CA::MetalDrawable*)Buffer.Drawable;

	if (Drawable->texture()->width() != State.Width || Drawable->texture()->height() != State.Height)
	{
		State.Width = Drawable->texture()->width();
		State.Height = Drawable->texture()->height();

		State.DepthTexture->release();
		MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::texture2DDescriptor(State.DepthFormat, State.Width, State.Height, false);
		descriptor->setUsage(MTL::TextureUsageRenderTarget);
		State.DepthTexture = State.Device->newTexture(descriptor);

		auto* DepthAttachment = State.DrawableRenderPass->depthAttachment();
		DepthAttachment->setTexture(State.DepthTexture);
	}

	static f32 Angle = 0.0f;
	Angle += 1.0f * DeltaTime;

	State.Projection = glm::perspectiveFov(
		glm::radians(90.0f), (f32)Drawable->texture()->width(), (f32)Drawable->texture()->height(), 0.01f * s_UnitsInMeter, 1000.0f * s_UnitsInMeter);
	State.View = glm::lookAtLH(State.CameraPosition * s_UnitsInMeter, State.CameraPosition * s_UnitsInMeter + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));

	MTL::CommandBuffer* Cmd = State.Queue->commandBuffer();

	{
		State.DrawableRenderPass->colorAttachments()->object(0)->setTexture(Drawable->texture());
		State.DrawableRenderPass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.08, 0.18, 0.8, 1.0));
		State.DrawableRenderPass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
		State.DrawableRenderPass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

		MTL::RenderCommandEncoder* Encoder = Cmd->renderCommandEncoder(State.DrawableRenderPass);
		Encoder->setRenderPipelineState(State.Pipeline);
		Encoder->setDepthStencilState(State.DepthStencilState);
		// Encoder->setTriangleFillMode(MTL::TriangleFillModeLines);

		for (auto& Mesh : State.Meshes)
		{
			auto Model = glm::translate(glm::mat4(1), Mesh.Position * s_UnitsInMeter) * glm::rotate(glm::mat4(1), -90.0f, glm::vec3(1.0f, 0.0f, 0.0f))
				* glm::rotate(glm::mat4(1), Angle, glm::vec3(0.0f, 0.0f, 1.0f));
			State.MVP = State.Projection * State.View * Model;

			Encoder->setVertexBytes(&State.MVP, sizeof(State.MVP), 10);
			Encoder->setVertexBuffers(
				Mesh.Render.VertexBuffers.data(), (NS::UInteger*)Mesh.Render.VertexBufferOffsets.data(), NS::Range::Make(0, Mesh.Render.VertexBuffers.size()));
			for (auto& Submesh : Mesh.Render.Submeshes)
			{
				Encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, Submesh.IndexCount, Submesh.IndexType, Submesh.IndexBuffer, 0);
			}
		}
		Encoder->endEncoding();
	}

	Cmd->presentDrawable(Drawable);
	Cmd->encodeSignalEvent(State.PacingEvent.get(), State.PacingTimeStampIndex);
	Cmd->commit();

	pool->release();
}
