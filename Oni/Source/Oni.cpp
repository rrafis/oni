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

struct FGameState
{
	MTL::Device* Device;
	MTL::CommandQueue* Queue;
	MTL::RenderPassDescriptor* DrawableRenderPass;
	MTL::RenderPipelineState* Pipeline;

	NS::SharedPtr<MTL::SharedEvent> PacingEvent;
	u64 PacingTimeStampIndex;
	u64 MaxFramesInFlight;
	u64 Frame;

	Engine::FMesh Mesh;

	glm::vec3 CameraPosition;
	glm::vec3 MeshPosition;

	glm::mat4 Model;
	glm::mat4 View;
	glm::mat4 Projection;

	glm::mat4 MVP;
};

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

	State.Device = MTL::CreateSystemDefaultDevice();
	State.Queue = State.Device->newCommandQueue();
	State.DrawableRenderPass = MTL::RenderPassDescriptor::alloc()->init();

	State.PacingEvent = NS::TransferPtr(State.Device->newSharedEvent());
	State.PacingTimeStampIndex = 0;
	State.MaxFramesInFlight = 3;

	State.CameraPosition = glm::vec3(0.0f, 0.0f, 1.0f);
	State.MeshPosition = glm::vec3(3.0f, 3.0f, 10.0f);

	State.Projection = glm::perspectiveFov(glm::radians(60.0f), (f32)Props.Width, (f32)Props.Height, 0.01f, 100.0f);
	State.View = glm::lookAtLH(State.CameraPosition, State.CameraPosition + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
	State.Model = glm::translate(glm::mat4(1), State.MeshPosition);

	State.MVP = State.Projection * State.View * State.Model;

	State.Mesh = Engine::CreateSphere(State.Device, 1.0f);

	{
		const char* triShaderSource = "#include <metal_stdlib>\n"
									  "using namespace metal;\n"
									  "struct VertexIn {\n"
									  "    float4 position [[attribute(0)]];\n"
									  "};\n"
									  "struct VSOut {\n"
									  "	float4 position [[position]];\n"
									  "};\n"
									  "vertex VSOut vertex_main(const VertexIn in [[stage_in]], constant float4x4 &MVP [[buffer(1)]]) {\n"
									  "	VSOut out;\n"
									  "	out.position = MVP * in.position;\n"
									  "	return out;\n"
									  "}\n"
									  "fragment float4 fragment_main(VSOut in [[stage_in]]) {\n"
									  "	return float4(in.position.x, 0.18, in.position.y, 1.0);\n"
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
		pipeDesc->setVertexDescriptor(State.Mesh.VertexDescriptor);
		pipeDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);
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

	MTL::CommandBuffer* Cmd = State.Queue->commandBuffer();

	CA::MetalDrawable* Drawable = (CA::MetalDrawable*)Buffer.Drawable;

	{
		State.DrawableRenderPass->colorAttachments()->object(0)->setTexture(Drawable->texture());
		State.DrawableRenderPass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.08, 0.18, 0.8, 1.0));
		State.DrawableRenderPass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
		State.DrawableRenderPass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

		MTL::RenderCommandEncoder* Encoder = Cmd->renderCommandEncoder(State.DrawableRenderPass);
		Encoder->setRenderPipelineState(State.Pipeline);
		Encoder->setTriangleFillMode(MTL::TriangleFillModeLines);

		Encoder->setVertexBuffer(State.Mesh.VertexBuffer, 0, 0);
		Encoder->setVertexBytes(&State.MVP, sizeof(State.MVP), 1);
		Encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, State.Mesh.IndexCount, State.Mesh.IndexType, State.Mesh.IndexBuffer, 0);

		Encoder->endEncoding();
	}

	Cmd->presentDrawable(Drawable);
	Cmd->encodeSignalEvent(State.PacingEvent.get(), State.PacingTimeStampIndex);
	Cmd->commit();

	pool->release();
}
