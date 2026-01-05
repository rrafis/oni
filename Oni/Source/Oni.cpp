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
	glm::vec3 Rotation;
	glm::vec3 Velocity;
	f32 PlayerAccelerationRate;
	f32 PlayerFrictionCoeficient;
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

	MTL::ResourceID DummyTexture;

	MTL::SamplerState* Sampler;
	MTL::Buffer* MaterialBuffer;
	MTL::Buffer* ArgumentBuffer;

	std::vector<MTL::Texture*> Textures;
	std::vector<Engine::Mesh::FRenderMaterial> Materials;

	NS::SharedPtr<MTL::SharedEvent> PacingEvent;
	u64 PacingTimeStampIndex;
	u64 MaxFramesInFlight;
	u64 Frame;

	FMesh* Player;
	FMesh* Capsule;
	std::vector<FMesh> Meshes;

	glm::vec3 CameraPosition;

	glm::mat4 View;
	glm::mat4 Projection;

	glm::mat4 MVP;
	f32 FOV;
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

auto CreateTexture(FGameState& State, MTL::Device* Device, FTexture RawTexture, const char* Name, bool bGenerateMips = true) -> MTL::ResourceID
{
	MTL::TextureDescriptor* TextureDesc =
		MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA8Unorm_sRGB, RawTexture.Width, RawTexture.Height, bGenerateMips);
	MTL::Texture* Texture = State.Device->newTexture(TextureDesc);

	// Upload texture data to VRAM
	MTL::Region region = MTL::Region(0, 0, 0, RawTexture.Width, RawTexture.Height, 1);
	NS::UInteger bytesPerRow = RawTexture.Channels * RawTexture.Width;
	Texture->replaceRegion(region, 0, (void*)RawTexture.Pixels.data(), bytesPerRow);
	Texture->setLabel(NS::String::alloc()->init(Name, NS::UTF8StringEncoding));

	// Generate mipmaps
	if (bGenerateMips)
	{
		MTL::CommandQueue* queue = Device->newCommandQueue();
		MTL::CommandBuffer* cmd = queue->commandBuffer();
		MTL::BlitCommandEncoder* encoder = cmd->blitCommandEncoder();
		encoder->generateMipmaps(Texture);
		encoder->endEncoding();
		cmd->commit();
		cmd->waitUntilCompleted();
	}

	State.Textures.emplace_back(Texture);
	return Texture->gpuResourceID();
}

auto LoadTexture(FGameState& State, const char* RootPath, const char* Name) -> MTL::ResourceID
{
	auto LoadPath = std::filesystem::current_path() / ContentPath / RootPath / Name;
	LoadPath.replace_extension(".png");
	auto RawTexture = LoadImage(LoadPath.c_str());
	return CreateTexture(State, State.Device, RawTexture, Name);
}

auto CreateDummyTexture(FGameState& State, const char* Name = "Dummy") -> MTL::ResourceID
{
	FTexture RawTexture;
	RawTexture.Width = 1;
	RawTexture.Height = 1;
	RawTexture.Channels = 4;
	// Set one pixel to 1,1,1,1
	RawTexture.Pixels.emplace_back(255);
	RawTexture.Pixels.emplace_back(255);
	RawTexture.Pixels.emplace_back(255);
	RawTexture.Pixels.emplace_back(255);

	return CreateTexture(State, State.Device, RawTexture, Name, false);
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
			RenderMat.BaseColorTex = LoadTexture(State, RootPath, Material.BaseColorTex.data());
		else
			RenderMat.BaseColorTex = State.DummyTexture;

		if (Material.MetallicRoughnessTex.size() > 0)
			RenderMat.MetallicRoughnessTex = LoadTexture(State, RootPath, Material.MetallicRoughnessTex.data());

		if (Material.NormalTex.size() > 0)
			RenderMat.NormalTex = LoadTexture(State, RootPath, Material.NormalTex.data());

		if (Material.EmissiveTex.size() > 0)
			RenderMat.EmissiveTex = LoadTexture(State, RootPath, Material.EmissiveTex.data());

		if (Material.OcclusionTex.size() > 0)
			RenderMat.OcclusionTex = LoadTexture(State, RootPath, Material.OcclusionTex.data());

		State.Materials.emplace_back(RenderMat);
	}

	return Result;
}

auto LoadBox(FGameState& State, MTL::Device* Device, glm::vec3 Extent, glm::uvec3 Segments) -> Engine::Mesh::FMeshRenderer
{
	auto Mesh = Engine::Mesh::MDLIO::ExportBox(Device, Extent, Segments);
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
		RenderMat.BaseColorTex = State.DummyTexture;
		RenderMat.MetallicRoughnessTex = State.DummyTexture;
		RenderMat.NormalTex = State.DummyTexture;
		RenderMat.EmissiveTex = State.DummyTexture;
		RenderMat.OcclusionTex = State.DummyTexture;
		State.Materials.emplace_back(RenderMat);
	}

	return Result;
}

auto LoadCapsule(FGameState& State, MTL::Device* Device, glm::vec3 Extent, glm::uvec3 Segments) -> Engine::Mesh::FMeshRenderer
{
	auto Mesh = Engine::Mesh::MDLIO::ExportCapsule(Device, Extent, Segments);
	auto Result = Engine::Mesh::Metal::ProcessMeshData(Device, Mesh);

	// HACK: Make stornger references for material index
	auto MaterailIndexOffset = State.Materials.size();
	for (auto& Submesh : Result.Submeshes)
	{
		Submesh.MaterialIndex += MaterailIndexOffset;
		Submesh.bWireframe = true;
	}

	for (auto& Material : Mesh.Materials)
	{
		Engine::Mesh::FRenderMaterial RenderMat;
		RenderMat.BaseColorFactor = Material.BaseColorFactor;
		RenderMat.EmissiveFactor = Material.EmissiveFactor;
		RenderMat.MetallicFactor = Material.MetallicFactor;
		RenderMat.RoughnessFactor = Material.RoughnessFactor;
		RenderMat.BaseColorTex = State.DummyTexture;
		RenderMat.MetallicRoughnessTex = State.DummyTexture;
		RenderMat.NormalTex = State.DummyTexture;
		RenderMat.EmissiveTex = State.DummyTexture;
		RenderMat.OcclusionTex = State.DummyTexture;
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

	State.DummyTexture = CreateDummyTexture(State);

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
	State.Meshes[0].Position = glm::vec3(0.0f, 0.0f, 0.0f);
	State.Meshes[0].Rotation.x = -90.f;
	State.Meshes[0].Render = LoadMesh(State, State.Device, "meshes/samanosuke_future", "Future_Samanosuke.obj");
	State.Meshes.emplace_back();
	State.Meshes[1].Position = glm::vec3(0.0f, 0.0f, 0.0f);
	State.Meshes[1].Rotation.x = -90.f;
	State.Meshes[1].Render = LoadMesh(State, State.Device, "meshes/samanosuke_weapons", "Tensou_lvl_3.obj");
	State.Meshes.emplace_back();
	State.Meshes[2].Position = glm::vec3(0.0f, -0.05f, 0.0f);
	State.Meshes[2].Render = LoadBox(State, State.Device, glm::vec3(100.0f, 0.1f, 100.0f) * s_UnitsInMeter, glm::uvec3(10, 1, 10));
	State.Meshes.emplace_back();
	State.Meshes[3].Position = glm::vec3(0.0f, 0.875f, 0.0f);
	State.Meshes[3].Render = LoadCapsule(State, State.Device, glm::vec3(0.25f, 1.75f, 0.25f) * s_UnitsInMeter, glm::uvec3(12, 4, 6));

	State.Player = &State.Meshes[0];
	State.Capsule = &State.Meshes[3];
	State.Player->PlayerAccelerationRate = 20.0f;
	State.Player->PlayerFrictionCoeficient = 5.0f;

	State.FOV = 60.f;
	State.Projection = glm::perspectiveFov(glm::radians(State.FOV), (f32)Props.Width, (f32)Props.Height, 0.01f * s_UnitsInMeter, 1000.0f * s_UnitsInMeter);
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

		State.Projection = glm::perspectiveFov(glm::radians(State.FOV), (f32)State.Width, (f32)State.Height, 0.01f * s_UnitsInMeter, 1000.0f * s_UnitsInMeter);
	}

	// Move player
	{
		auto LeftStickAcceleration = glm::vec2(Gamepad.GamepadThumbstickLeft.X, Gamepad.GamepadThumbstickLeft.Y);
		if (Gamepad.GamepadDpadUp.bPressed)
		{
			LeftStickAcceleration.y = 1.0f;
		}
		if (Gamepad.GamepadDpadDown.bPressed)
		{
			LeftStickAcceleration.y = -1.0f;
		}
		if (Gamepad.GamepadDpadRight.bPressed)
		{
			LeftStickAcceleration.x = 1.0f;
		}
		if (Gamepad.GamepadDpadLeft.bPressed)
		{
			LeftStickAcceleration.x = -1.0f;
		}
		if (LeftStickAcceleration.x != 0 || LeftStickAcceleration.y != 0.0)
		{
			LeftStickAcceleration = glm::normalize(LeftStickAcceleration);
		}
		LeftStickAcceleration *= State.Player->PlayerAccelerationRate;

		glm::vec3 frictionAcceleration = State.Player->PlayerFrictionCoeficient * (-State.Player->Velocity);
		glm::vec3 Acceleration = glm::vec3(LeftStickAcceleration.x, 0.0f, LeftStickAcceleration.y) + frictionAcceleration;

		glm::vec3 MoveDelta = 0.5f * Acceleration * DeltaTime * DeltaTime + State.Player->Velocity * DeltaTime;
		State.Player->Velocity = Acceleration * DeltaTime + State.Player->Velocity;

		State.Player->Position += MoveDelta;
	}

	// Make camera to follow player
	{
		State.CameraPosition = State.Player->Position + glm::vec3(-0.25f, 1.5f, -2.5f);
		auto LookAtPosition = (State.Player->Position + glm::vec3(-0.25f, 1.25f, 0.0));
		State.View = glm::lookAtLH(State.CameraPosition * s_UnitsInMeter, LookAtPosition * s_UnitsInMeter, glm::vec3(0, 1, 0));
	}

	State.Capsule->Position = State.Player->Position + glm::vec3(0.0f, 0.875f, 0.0f);

	MTL::CommandBuffer* Cmd = State.Queue->commandBuffer();

	{
		State.DrawableRenderPass->colorAttachments()->object(0)->setTexture(Drawable->texture());
		State.DrawableRenderPass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.08, 0.18, 0.8, 1.0));
		State.DrawableRenderPass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
		State.DrawableRenderPass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

		MTL::RenderCommandEncoder* Encoder = Cmd->renderCommandEncoder(State.DrawableRenderPass);
		Encoder->setRenderPipelineState(State.Pipeline);
		Encoder->setDepthStencilState(State.DepthStencilState);

		for (auto& Tex : State.Textures)
		{
			Encoder->useResource(Tex, MTL::ResourceUsageSample);
		}
		Encoder->useResource(State.MaterialBuffer, MTL::ResourceUsageRead);

		Encoder->setFragmentBuffer(State.MaterialBuffer, 0, 11);
		Encoder->setFragmentBuffer(State.ArgumentBuffer, 0, 12);

		for (auto& Mesh : State.Meshes)
		{
			auto Model = glm::translate(glm::mat4(1), Mesh.Position * s_UnitsInMeter)
				* glm::rotate(glm::mat4(1), glm::radians(Mesh.Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
			State.MVP = State.Projection * State.View * Model;

			Encoder->setVertexBytes(&State.MVP, sizeof(State.MVP), 10);
			Encoder->setVertexBuffers(
				Mesh.Render.VertexBuffers.data(), (NS::UInteger*)Mesh.Render.VertexBufferOffsets.data(), NS::Range::Make(0, Mesh.Render.VertexBuffers.size()));
			for (auto& Submesh : Mesh.Render.Submeshes)
			{
				Encoder->setTriangleFillMode(Submesh.bWireframe ? MTL::TriangleFillModeLines : MTL::TriangleFillModeFill);
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
