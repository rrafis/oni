#include <Platform.h>

#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

	MTL::SamplerState* Sampler;
	MTL::Buffer* MaterialBuffer;
	MTL::Buffer* ArgumentBuffer;

	std::vector<MTL::Texture*> Textures;
	std::vector<Engine::Mesh::FRenderMaterial> Materials;

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

struct FTexture
{
	u32 Width, Height;
	u8 Channels;
	std::vector<u8> Pixels;
};

auto LoadImage(const char* path) -> FTexture
{
	stbi_set_flip_vertically_on_load(true);

	int width, height, channels;
	unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
	channels = 4;

	if (data == nullptr)
	{
		// TODO: Handle error case
		auto v = *data;
	}

	auto size = width * height * channels;

	FTexture Texture;
	Texture.Width = width;
	Texture.Height = height;
	Texture.Channels = channels;
	Texture.Pixels.reserve(size);
	for (u32 index = 0; index < size; index += 4)
	{
		Texture.Pixels.emplace_back(data[index + 0]);
		Texture.Pixels.emplace_back(data[index + 1]);
		Texture.Pixels.emplace_back(data[index + 2]);
		Texture.Pixels.emplace_back(data[index + 3]);
	}

	// std::cout << "Image loaded: " << width << "x" << height << " with " << channels << " channels." << std::endl;

	// Free memory when done
	stbi_image_free(data);

	return Texture;
}

auto CreateTexture(FGameState& State, MTL::Device* Device, const char* RootPath, const char* Name) -> MTL::ResourceID
{
	auto LoadPath = std::filesystem::current_path() / ContentPath / RootPath / Name;
	LoadPath.replace_extension(".png");
	auto RawTexture = LoadImage(LoadPath.c_str());

	MTL::TextureDescriptor* TextureDesc =
		MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA8Unorm_sRGB, RawTexture.Width, RawTexture.Height, true);
	MTL::Texture* Texture = State.Device->newTexture(TextureDesc);

	// Upload texture data to VRAM
	MTL::Region region = MTL::Region(0, 0, 0, RawTexture.Width, RawTexture.Height, 1);
	NS::UInteger bytesPerRow = RawTexture.Channels * RawTexture.Width;
	Texture->replaceRegion(region, 0, (void*)RawTexture.Pixels.data(), bytesPerRow);
	Texture->setLabel(NS::String::alloc()->init(Name, NS::UTF8StringEncoding));

	// Generate mipmaps
	MTL::CommandQueue* queue = Device->newCommandQueue();
	MTL::CommandBuffer* cmd = queue->commandBuffer();
	MTL::BlitCommandEncoder* encoder = cmd->blitCommandEncoder();
	encoder->generateMipmaps(Texture);
	encoder->endEncoding();
	cmd->commit();
	cmd->waitUntilCompleted();

	State.Textures.emplace_back(Texture);
	return Texture->gpuResourceID();
}

auto LoadMesh(FGameState& State, MTL::Device* Device, const char* RootPath, const char* Name) -> Engine::Mesh::FMeshRenderer
{
	auto LoadPath = std::filesystem::path() / ContentPath / RootPath / Name;
	auto Mesh = Engine::Mesh::MDLIO::ExportMeshFromPath(Device, LoadPath.c_str());

	auto Result = Engine::Mesh::Metal::ProcessMeshData(Device, Mesh);

	// HACK: Make stornger references for material index
	auto MaterailIndexOffset = State.Materials.size();
	for (auto& Submesh : Result.Submeshes)
	{
		Submesh.MaterialIndex += MaterailIndexOffset;
	}

	for (auto& Material : Mesh.Materials)
	{
		Engine::Mesh::FRenderMaterial RenderMat;
		RenderMat.BaseColorFactor = Material.BaseColorFactor;
		RenderMat.EmissiveFactor = Material.EmissiveFactor;
		RenderMat.MetallicFactor = Material.MetallicFactor;
		RenderMat.RoughnessFactor = Material.RoughnessFactor;
		if (Material.BaseColorTex.size() > 0)
			RenderMat.BaseColorTex = CreateTexture(State, Device, RootPath, Material.BaseColorTex.data());
		if (Material.MetallicRoughnessTex.size() > 0)
			RenderMat.MetallicRoughnessTex = CreateTexture(State, Device, RootPath, Material.MetallicRoughnessTex.data());
		if (Material.NormalTex.size() > 0)
			RenderMat.NormalTex = CreateTexture(State, Device, RootPath, Material.NormalTex.data());
		if (Material.EmissiveTex.size() > 0)
			RenderMat.EmissiveTex = CreateTexture(State, Device, RootPath, Material.EmissiveTex.data());
		if (Material.OcclusionTex.size() > 0)
			RenderMat.OcclusionTex = CreateTexture(State, Device, RootPath, Material.OcclusionTex.data());
		State.Materials.emplace_back(RenderMat);
	}

	return Result;
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

		MTL::TextureDescriptor* DepthTextureDesc = MTL::TextureDescriptor::texture2DDescriptor(State.DepthFormat, State.Width, State.Height, false);
		DepthTextureDesc->setUsage(MTL::TextureUsageRenderTarget);
		State.DepthTexture = State.Device->newTexture(DepthTextureDesc);

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
	State.Meshes[0].Render = LoadMesh(State, State.Device, "meshes/samanosuke_future", "Future_Samanosuke.obj");
	State.Meshes.emplace_back();
	State.Meshes[1].Position = glm::vec3(0.0f, -0.5f, 2.0f);
	State.Meshes[1].Render = LoadMesh(State, State.Device, "meshes/samanosuke_weapons", "Tensou_lvl_3.obj");

	State.Projection = glm::perspectiveFov(glm::radians(90.0f), (f32)Props.Width, (f32)Props.Height, 0.01f * s_UnitsInMeter, 100.0f * s_UnitsInMeter);
	State.View = glm::lookAtLH(State.CameraPosition * s_UnitsInMeter, State.CameraPosition * s_UnitsInMeter + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));

	State.MaterialBuffer =
		State.Device->newBuffer(State.Materials.data(), State.Materials.size() * sizeof(Engine::Mesh::FRenderMaterial), MTL::ResourceStorageModeShared);
	{
		NS::Error* error;
		NS::String* ShaderPath = NS::String::alloc()->init("Content/Shaders/BuiltIn.MaterialShader.metallib", NS::StringEncoding::UTF8StringEncoding);
		auto Shader = State.Device->newLibrary(ShaderPath, &error);
		if (!Shader)
		{
			if (error)
			{
				NS::String* desc = error->localizedDescription();
				printf("Shader compile error: %s\n", desc->utf8String());
			}
			else
			{
				printf("Shader compile failed with unknown error.\n");
			}
		}

		auto vertFunc = Shader->newFunction(NS::String::alloc()->init("vertex_main", NS::StringEncoding::UTF8StringEncoding));
		auto fragFunc = Shader->newFunction(NS::String::alloc()->init("fragment_main", NS::StringEncoding::UTF8StringEncoding));

		MTL::SamplerDescriptor* sdesc = MTL::SamplerDescriptor::alloc()->init();
		sdesc->setMinFilter(MTL::SamplerMinMagFilterNearest);
		sdesc->setMagFilter(MTL::SamplerMinMagFilterNearest);
		sdesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
		sdesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
		sdesc->setSupportArgumentBuffers(true);
		State.Sampler = State.Device->newSamplerState(sdesc);
		sdesc->release();

		auto ArgEncoder = fragFunc->newArgumentEncoder(12);
		State.ArgumentBuffer = State.Device->newBuffer(ArgEncoder->encodedLength(), MTL::ResourceStorageModeShared);
		ArgEncoder->setArgumentBuffer(State.ArgumentBuffer, 0);
		ArgEncoder->setSamplerState(State.Sampler, 0);
		ArgEncoder->setBuffer(State.MaterialBuffer, 0, 1);
		ArgEncoder->release();

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
		MTL::TextureDescriptor* DepthTextureDesc = MTL::TextureDescriptor::texture2DDescriptor(State.DepthFormat, State.Width, State.Height, false);
		DepthTextureDesc->setUsage(MTL::TextureUsageRenderTarget);
		State.DepthTexture = State.Device->newTexture(DepthTextureDesc);

		auto* DepthAttachment = State.DrawableRenderPass->depthAttachment();
		DepthAttachment->setTexture(State.DepthTexture);
	}

	static f32 Angle = glm::radians(180.0f);
	Angle += glm::radians(20.0f) * DeltaTime;

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

		for (auto& Tex : State.Textures)
		{
			Encoder->useResource(Tex, MTL::ResourceUsageSample);
		}
		Encoder->useResource(State.MaterialBuffer, MTL::ResourceUsageRead);

		Encoder->setFragmentBuffer(State.MaterialBuffer, 0, 11);
		Encoder->setFragmentBuffer(State.ArgumentBuffer, 0, 12);

		for (auto& Mesh : State.Meshes)
		{
			auto Model = glm::translate(glm::mat4(1), Mesh.Position * s_UnitsInMeter) * glm::rotate(glm::mat4(1), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
				* glm::rotate(glm::mat4(1), Angle, glm::vec3(0.0f, 0.0f, 1.0f));
			State.MVP = State.Projection * State.View * Model;

			Encoder->setVertexBytes(&State.MVP, sizeof(State.MVP), 10);
			Encoder->setVertexBuffers(
				Mesh.Render.VertexBuffers.data(), (NS::UInteger*)Mesh.Render.VertexBufferOffsets.data(), NS::Range::Make(0, Mesh.Render.VertexBuffers.size()));
			for (auto& Submesh : Mesh.Render.Submeshes)
			{
				Encoder->setFragmentBytes(&Submesh.MaterialIndex, sizeof(u32), 13);
				Encoder->setCullMode(Submesh.bDoubleSided ? MTL::CullModeNone : MTL::CullModeBack);
				Encoder->setFrontFacingWinding(Submesh.bWindingOrderClockWise ? MTL::WindingClockwise : MTL::WindingCounterClockwise);
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
