#include <Platform.h>

#include <memory>

// Imple for the metal-cpp
#define CA_PRIVATE_IMPLEMENTATION
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTLFX_PRIVATE_IMPLEMENTATION

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

	{
		const char* triShaderSource = "#include <metal_stdlib>\n"
									  "using namespace metal;\n"
									  "struct VSOut {\n"
									  "	float4 position [[position]];\n"
									  "};\n"
									  "vertex VSOut vertex_main(uint vid [[vertex_id]]) {\n"
									  "	float2 pos;\n"
									  "	if (vid == 0) pos = float2(-0.5, -0.5);\n"
									  "	if (vid == 1) pos = float2( 0.5, -0.5);\n"
									  "	if (vid == 2) pos = float2( 0.0,  0.5);\n"
									  "	VSOut out;\n"
									  "	out.position = float4(pos, 0.0, 1.0);\n"
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
		Encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
		Encoder->endEncoding();
	}

	Cmd->presentDrawable(Drawable);
	Cmd->encodeSignalEvent(State.PacingEvent.get(), State.PacingTimeStampIndex);
	Cmd->commit();

	pool->release();
}
