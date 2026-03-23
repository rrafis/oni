#include <Platform.h>

#include <memory>

#include "Types.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "MeshUtils.h"
#include "PlayerController.h"
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <MetalFX/MetalFX.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <cstdarg>
#include <filesystem>
#include <iostream>
#include <thread>

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

namespace ph
{
	// Callback for traces, connect this to your own trace function if you have one
	static void TraceImpl(const char* inFMT, ...)
	{
		// Format the message
		va_list list;
		va_start(list, inFMT);
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), inFMT, list);
		va_end(list);

		// Print to the TTY
		std::cout << buffer << std::endl;
	}

#ifdef JPH_ENABLE_ASSERTS

	// Callback for asserts, connect this to your own assert handler if you have one
	static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine)
	{
		// Print to the TTY
		std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << std::endl;

		// Breakpoint
		return true;
	};

#endif // JPH_ENABLE_ASSERTS

	// Layer that objects can be in, determines which other objects it can collide with
	// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
	// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
	// but only if you do collision testing).
	namespace Layers
	{
		static constexpr JPH::ObjectLayer NON_MOVING = 0;
		static constexpr JPH::ObjectLayer MOVING = 1;
		static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
	}; // namespace Layers

	/// Class that determines if two object layers can collide
	class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
		{
			switch (inObject1)
			{
				case Layers::NON_MOVING:
					return inObject2 == Layers::MOVING; // Non moving only collides with moving
				case Layers::MOVING:
					return true; // Moving collides with everything
				default:
					JPH_ASSERT(false);
					return false;
			}
		}
	};

	// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
	// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
	// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
	// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
	// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
	namespace BroadPhaseLayers
	{
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
		static constexpr uint NUM_LAYERS(2);
	}; // namespace BroadPhaseLayers

	// BroadPhaseLayerInterface implementation
	// This defines a mapping between object and broadphase layers.
	class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
	{
	public:
		BPLayerInterfaceImpl()
		{
			// Create a mapping table from object to broad phase layer
			mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}

		virtual uint GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_LAYERS;
		}

		virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
		{
			JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
			return mObjectToBroadPhase[inLayer];
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
		{
			switch ((JPH::BroadPhaseLayer::Type)inLayer)
			{
				case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
					return "NON_MOVING";
				case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
					return "MOVING";
				default:
					JPH_ASSERT(false);
					return "INVALID";
			}
		}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	private:
		JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
	};

	/// Class that determines if an object layer can collide with a broadphase layer
	class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
		{
			switch (inLayer1)
			{
				case Layers::NON_MOVING:
					return inLayer2 == BroadPhaseLayers::MOVING;
				case Layers::MOVING:
					return true;
				default:
					JPH_ASSERT(false);
					return false;
			}
		}
	};

	// An example contact listener
	class MyContactListener : public JPH::ContactListener
	{
	public:
		// See: ContactListener
		virtual JPH::ValidateResult OnContactValidate(
			const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override
		{
			// std::cout << "Contact validate callback" << std::endl;

			// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
			return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
		}

		virtual void OnContactAdded(
			const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override
		{
			// std::cout << "A contact was added" << std::endl;
		}

		virtual void OnContactPersisted(
			const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override
		{
			// std::cout << "A contact was persisted" << std::endl;
		}

		virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
		{
			// std::cout << "A contact was removed" << std::endl;
		}
	};

	// An example activation listener
	class MyBodyActivationListener : public JPH::BodyActivationListener
	{
	public:
		virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
		{
			// std::cout << "A body got activated" << std::endl;
		}

		virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
		{
			// std::cout << "A body went to sleep" << std::endl;
		}
	};

} // namespace ph

struct FGameState
{
	MTL::Device* Device;
	NS::SharedPtr<MTL4::CommandQueue> Queue;
	NS::SharedPtr<MTL4::CommandAllocator> Allocator;
	NS::SharedPtr<MTL4::RenderPassDescriptor> DrawableRenderPass;
	NS::SharedPtr<MTL::ResidencySet> ResidencySet;

	NS::SharedPtr<MTL4::Compiler> ShaderCompiler;
	NS::SharedPtr<MTL4::CompilerTaskOptions> ShaderCompOptions;
	NS::SharedPtr<MTL::RenderPipelineState> Pipeline;
	NS::SharedPtr<MTL::RenderPipelineState> SkPipeline;

	MTL::PixelFormat DepthFormat;
	MTL::Texture* DepthTexture;
	MTL::DepthStencilState* DepthStencilState;

	MTL::ResourceID DummyTexture;

	MTL::SamplerState* Sampler;
	MTL::Buffer* MaterialBuffer;

	std::vector<MTL::Texture*> Textures;
	std::vector<Engine::Mesh::FRenderMaterial> Materials;

	NS::SharedPtr<MTL::SharedEvent> PacingEvent;
	u64 PacingTimeStampIndex;
	u64 MaxFramesInFlight;
	u64 Frame;

	Engine::FTransform* Player;
	Engine::FTransform* Capsule;

	std::vector<Engine::Mesh::FMeshData> NewMeshes;
	std::vector<Engine::Mesh::FSkeleton> NewSkeletons;
	std::vector<Engine::Mesh::FMaterial> NewMaterials;
	std::unordered_map<std::string, u32> SkeletonMap;
	std::unordered_map<std::string, u32> MaterialMap;

	std::vector<Engine::Mesh::FSkinnedMeshRenderer> SkinnedMeshRenderers;
	std::vector<Engine::Mesh::FSkeletonRenderData> SkeletonRenderers;
	std::vector<Engine::Mesh::FMeshRenderer> MeshRenderers;
	std::vector<Engine::FTransform> SkinnedMeshTransforms;
	std::vector<Engine::FTransform> MeshTransforms;

	glm::vec3 CameraPosition;

	glm::mat4 View;
	glm::mat4 Projection;

	f32 FOV;
	u32 Width;
	u32 Height;

	// Camera control
	float CameraYaw;
	float CameraPitch;

	JPH::PhysicsSystem PhysicsSystem;
	JPH::TempAllocatorImpl* TempAllocator;
	JPH::JobSystemThreadPool JobSystem;
	JPH::BodyID PlayerCapsuleID;
	JPH::Body* Floor;
	bool bWasJumpPressed;
	ph::MyBodyActivationListener BodyActivationListener;
	ph::MyContactListener ContactListener;
	ph::ObjectLayerPairFilterImpl ObjectVsObjectLayerFilter;
	ph::ObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadphaseLayerFilter;
	ph::BPLayerInterfaceImpl BroadPhaseLayerInterface;

	glm::vec3 LastRenderedPlayerPos; // Last smoothed render position for player (used to reduce jitter)
};

#ifdef _RE_XCODE
constexpr auto ContentPath = "../../../Content";
#else
constexpr auto ContentPath = "Content";
#endif

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

auto LoadMesh(FGameState& State, MTL::Device* Device, std::vector<Engine::Mesh::FMeshData> Meshes, std::vector<Engine::Mesh::FMaterial> Materials = {},
	const char* RootPath = nullptr) -> std::vector<Engine::Mesh::FMeshRenderer>
{
	for (auto& Mesh : Meshes)
	{
		State.NewMeshes.emplace_back(Mesh);
	}

	u32 Offset = State.MaterialMap.size();
	for (u32 Idx = 0; Idx < Materials.size(); Idx++)
	{
		auto& mat = State.NewMaterials.emplace_back(Materials[Idx]);

		Engine::Mesh::FRenderMaterial RenderMat;
		RenderMat.BaseColorFactor = mat.BaseColorFactor;
		RenderMat.EmissiveFactor = mat.EmissiveFactor;
		RenderMat.MetallicFactor = mat.MetallicFactor;
		RenderMat.RoughnessFactor = mat.RoughnessFactor;

		if (mat.BaseColorTex.size() > 0)
			RenderMat.BaseColorTex = LoadTexture(State, RootPath, mat.BaseColorTex.data());
		else
			RenderMat.BaseColorTex = State.DummyTexture;

		if (mat.MetallicRoughnessTex.size() > 0)
			RenderMat.MetallicRoughnessTex = LoadTexture(State, RootPath, mat.MetallicRoughnessTex.data());

		if (mat.NormalTex.size() > 0)
			RenderMat.NormalTex = LoadTexture(State, RootPath, mat.NormalTex.data());

		if (mat.EmissiveTex.size() > 0)
			RenderMat.EmissiveTex = LoadTexture(State, RootPath, mat.EmissiveTex.data());

		if (mat.OcclusionTex.size() > 0)
			RenderMat.OcclusionTex = LoadTexture(State, RootPath, mat.OcclusionTex.data());

		State.Materials.emplace_back(RenderMat);

		State.MaterialMap[mat.Name] = Offset + Idx;
	}

	std::vector<Engine::Mesh::FMeshRenderer> Result;
	for (auto& MeshData : Meshes)
	{
		Result.emplace_back(Engine::Mesh::Metal::ProcessMeshData(Device, MeshData, State.MaterialMap));
	}

	return Result;
}

auto LoadMesh(FGameState& State, MTL::Device* Device, const char* RootPath, const char* Name) -> std::vector<Engine::Mesh::FMeshRenderer>
{
	auto LoadPath = std::filesystem::path() / ContentPath / RootPath / Name;
	auto ext = LoadPath.extension();
	if (!LoadPath.has_extension())
	{
		return {};
	}

	std::vector<Engine::Mesh::FMeshData> Meshes;
	std::vector<Engine::Mesh::FMaterial> Materials;
	if (LoadPath.extension() == ".gltf" || LoadPath.extension() == ".glb")
	{
		auto Data = Engine::Mesh::gltf::LoadData(LoadPath.c_str());
		Meshes = Engine::Mesh::gltf::LoadMesh(Data);
		Materials = Engine::Mesh::gltf::LoadMaterial(Data);
		Engine::Mesh::gltf::FreeData(Data);
	}
	else
	{
		auto Data = Engine::Mesh::mdl::LoadData(Device, LoadPath.c_str());
		Meshes = Engine::Mesh::mdl::LoadMesh(Device, Data);
		Materials = Engine::Mesh::mdl::LoadMaterial(Device, Data);
		Engine::Mesh::mdl::FreeData(Data);
	}

	return LoadMesh(State, Device, Meshes, Materials, RootPath);
}

auto LoadMeshSkinned(FGameState& State, MTL::Device* Device, std::vector<Engine::Mesh::FMeshData> Meshes, std::vector<Engine::Mesh::FMaterial> Materials = {},
	const char* RootPath = nullptr, std::vector<Engine::Mesh::FSkeleton> Skeletons = {}) -> std::vector<Engine::Mesh::FSkinnedMeshRenderer>
{
	for (auto& Mesh : Meshes)
	{
		State.NewMeshes.emplace_back(Mesh);
	}

	u32 Offset = State.SkeletonMap.size();
	for (u32 Idx = 0; Idx < Skeletons.size(); Idx++)
	{
		auto& sk = State.NewSkeletons.emplace_back(Skeletons[Idx]);
		State.SkeletonMap[sk.Name] = Offset + Idx;
	}

	Offset = State.MaterialMap.size();
	for (u32 Idx = 0; Idx < Materials.size(); Idx++)
	{
		auto& mat = State.NewMaterials.emplace_back(Materials[Idx]);

		Engine::Mesh::FRenderMaterial RenderMat;
		RenderMat.BaseColorFactor = mat.BaseColorFactor;
		RenderMat.EmissiveFactor = mat.EmissiveFactor;
		RenderMat.MetallicFactor = mat.MetallicFactor;
		RenderMat.RoughnessFactor = mat.RoughnessFactor;

		if (mat.BaseColorTex.size() > 0)
			RenderMat.BaseColorTex = LoadTexture(State, RootPath, mat.BaseColorTex.data());
		else
			RenderMat.BaseColorTex = State.DummyTexture;

		if (mat.MetallicRoughnessTex.size() > 0)
			RenderMat.MetallicRoughnessTex = LoadTexture(State, RootPath, mat.MetallicRoughnessTex.data());

		if (mat.NormalTex.size() > 0)
			RenderMat.NormalTex = LoadTexture(State, RootPath, mat.NormalTex.data());

		if (mat.EmissiveTex.size() > 0)
			RenderMat.EmissiveTex = LoadTexture(State, RootPath, mat.EmissiveTex.data());

		if (mat.OcclusionTex.size() > 0)
			RenderMat.OcclusionTex = LoadTexture(State, RootPath, mat.OcclusionTex.data());

		State.Materials.emplace_back(RenderMat);

		State.MaterialMap[mat.Name] = Offset + Idx;
	}

	for (auto& Sk : Skeletons)
	{
		State.SkeletonRenderers.emplace_back(Engine::Mesh::Metal::ProcessSkeleton(Device, Sk));
	}

	std::vector<Engine::Mesh::FSkinnedMeshRenderer> Result;
	for (auto& MeshData : Meshes)
	{
		Result.emplace_back(
			Engine::Mesh::Metal::ProcessMeshData(Device, MeshData, State.MaterialMap, State.SkeletonRenderers[State.SkeletonMap[MeshData.SkeletonName]]));
	}

	return Result;
}
auto LoadMeshSkinned(FGameState& State, MTL::Device* Device, const char* RootPath, const char* Name) -> std::vector<Engine::Mesh::FSkinnedMeshRenderer>
{
	auto LoadPath = std::filesystem::path() / ContentPath / RootPath / Name;
	auto ext = LoadPath.extension();
	if (!LoadPath.has_extension())
	{
		return {};
	}

	std::vector<Engine::Mesh::FMeshData> Meshes;
	std::vector<Engine::Mesh::FSkeleton> Skeletons;
	std::vector<Engine::Mesh::FMaterial> Materials;
	if (LoadPath.extension() == ".gltf" || LoadPath.extension() == ".glb")
	{
		auto Data = Engine::Mesh::gltf::LoadData(LoadPath.c_str());
		Meshes = Engine::Mesh::gltf::LoadMesh(Data);
		Skeletons = Engine::Mesh::gltf::LoadSkeleton(Data);
		Materials = Engine::Mesh::gltf::LoadMaterial(Data);
		Engine::Mesh::gltf::FreeData(Data);
	}

	return LoadMeshSkinned(State, Device, Meshes, Materials, RootPath, Skeletons);
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
	State.Queue = NS::TransferPtr(State.Device->newMTL4CommandQueue());
	State.Allocator = NS::TransferPtr(State.Device->newCommandAllocator());
	State.DrawableRenderPass = NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());

	MTL::ResidencySetDescriptor* ResidencySetDesc = MTL::ResidencySetDescriptor::alloc()->init();
	State.ResidencySet = NS::TransferPtr(State.Device->newResidencySet(ResidencySetDesc, nullptr));

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

	// Setup a defualt material
	{
		auto& mat = State.NewMaterials.emplace_back(Engine::Mesh::FMaterial{ .Name = "Default" });
		State.MaterialMap[mat.Name] = State.NewMaterials.size() - 1;

		Engine::Mesh::FRenderMaterial RenderMat;
		RenderMat.BaseColorFactor = mat.BaseColorFactor;
		RenderMat.EmissiveFactor = mat.EmissiveFactor;
		RenderMat.MetallicFactor = mat.MetallicFactor;
		RenderMat.RoughnessFactor = mat.RoughnessFactor;
		State.Materials.emplace_back(RenderMat);
	}

	{
		auto Meshes = LoadMesh(State, State.Device, "meshes/samanosuke_future", "Future_Samanosuke.obj");
		for (auto& Mesh : Meshes)
		{
			State.MeshRenderers.emplace_back(Mesh);
			State.MeshTransforms.emplace_back(Engine::FTransform{ .Position = glm::vec3(0.0f, 0.0f, 0.0f),
				.Rotation = glm::quat(glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f)),
				.Scale = glm::vec3(1.0f / 30.0f) });
		}
	}

	{
		auto Meshes = LoadMesh(State, State.Device, "meshes/samanosuke_weapons", "Tensou_lvl_3.obj");
		for (auto& Mesh : Meshes)
		{
			State.MeshRenderers.emplace_back(Mesh);
			State.MeshTransforms.emplace_back(
				Engine::FTransform{ .Rotation = glm::quat(glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f)), .Scale = glm::vec3(1.0f / 30.0f) });
		}
	}

	{
		auto Box = Engine::Mesh::mdl::LoadBox("Test Box", State.Device, glm::vec3(100.0f, 1.0f, 100.f), glm::uvec3(10, 1, 10));
		auto Meshes = LoadMesh(State, State.Device, { Box });
		for (auto& Mesh : Meshes)
		{
			auto& Render = State.MeshRenderers.emplace_back(Mesh);
			Render.Submeshes[0].bWireframe = true;
			State.MeshTransforms.emplace_back(Engine::FTransform{ .Position = glm::vec3(0.0f, -0.5f, 0.0f) });
		}
	}

	{
		auto Capsule = Engine::Mesh::mdl::LoadCapsule("Test Capsule", State.Device, glm::vec3(0.25f, 1.75f, 0.25f), glm::uvec3(12, 4, 6));
		auto Meshes = LoadMesh(State, State.Device, { Capsule });
		for (auto& Mesh : Meshes)
		{
			auto& Render = State.MeshRenderers.emplace_back(Mesh);
			Render.Submeshes[0].bWireframe = true;
			State.MeshTransforms.emplace_back(Engine::FTransform{ .Position = glm::vec3(0.0f, 0.875f, 0.0f) });
		}
	}

	{
		auto Meshes = LoadMeshSkinned(State, State.Device, "meshes", "mannequiny-0.3.0.glb");
		for (auto& Mesh : Meshes)
		{
			State.SkinnedMeshRenderers.emplace_back(Mesh);
			State.SkinnedMeshTransforms.emplace_back(Engine::FTransform{ .Position = glm::vec3(0.0f, 0.0f, 0.0f),
				.Rotation = glm::quat(glm::vec3(0.0f, glm::radians(-180.0f), 0.0f)),
				.Scale = glm::vec3(1.0f / 30.0f) });
		}
	}

	State.Player = &State.MeshTransforms[0];
	State.Capsule = &State.MeshTransforms[3];
	State.bWasJumpPressed = false;

	// Initialize last rendered pos to current player position to avoid snap on first frame
	State.LastRenderedPlayerPos = State.Player->Position;

	State.FOV = 60.f;
	State.Projection = glm::perspectiveFov(glm::radians(State.FOV), (f32)Props.Width, (f32)Props.Height, 0.01f, 1000.0f);
	State.View = glm::lookAtLH(State.CameraPosition, State.CameraPosition + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));

	// Initialize camera orientation
	State.CameraYaw = 0.0f;
	State.CameraPitch = 10.0f; // degrees, slight downward angle

	{
		MTL::SamplerDescriptor* sdesc = MTL::SamplerDescriptor::alloc()->init();
		sdesc->setMinFilter(MTL::SamplerMinMagFilterNearest);
		sdesc->setMagFilter(MTL::SamplerMinMagFilterNearest);
		sdesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
		sdesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
		sdesc->setSupportArgumentBuffers(true);
		State.Sampler = State.Device->newSamplerState(sdesc);
		sdesc->release();
	}

	State.MaterialBuffer =
		State.Device->newBuffer(State.Materials.data(), State.Materials.size() * sizeof(Engine::Mesh::FRenderMaterial), MTL::ResourceStorageModeShared);
	{
		NS::Error* error;

		State.ShaderCompOptions = NS::RetainPtr(MTL4::CompilerTaskOptions::alloc()->init());
		NS::SharedPtr<MTL4::CompilerDescriptor> CompilerDesc = NS::RetainPtr(MTL4::CompilerDescriptor::alloc()->init());
		State.ShaderCompiler = NS::RetainPtr(State.Device->newCompiler(CompilerDesc.get(), &error));
		if (!State.ShaderCompiler)
		{
			NS::String* desc = error->localizedDescription();
			printf("Compiler create error: %s\n", desc->utf8String());
		}

		auto LoadPath = std::filesystem::path() / ContentPath / "Shaders" / "BuiltIn.MaterialShader.metallib";
		NS::String* ShaderPath = NS::String::alloc()->init(LoadPath.c_str(), NS::StringEncoding::UTF8StringEncoding);
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

		MTL4::LibraryFunctionDescriptor* vertFuncDesc = MTL4::LibraryFunctionDescriptor::alloc()->init();
		vertFuncDesc->setLibrary(Shader);
		vertFuncDesc->setName(NS::String::alloc()->init("vertex_main", NS::StringEncoding::UTF8StringEncoding));

		MTL4::LibraryFunctionDescriptor* vertSkFuncDesc = MTL4::LibraryFunctionDescriptor::alloc()->init();
		vertSkFuncDesc->setLibrary(Shader);
		vertSkFuncDesc->setName(NS::String::alloc()->init("vertex_sk_main", NS::StringEncoding::UTF8StringEncoding));

		MTL4::LibraryFunctionDescriptor* fragFuncDesc = MTL4::LibraryFunctionDescriptor::alloc()->init();
		fragFuncDesc->setLibrary(Shader);
		fragFuncDesc->setName(NS::String::alloc()->init("fragment_main", NS::StringEncoding::UTF8StringEncoding));

		MTL4::RenderPipelineDescriptor* pipeDesc = MTL4::RenderPipelineDescriptor::alloc()->init();
		pipeDesc->setVertexFunctionDescriptor(vertFuncDesc);
		pipeDesc->setFragmentFunctionDescriptor(fragFuncDesc);
		pipeDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);

		State.Pipeline = NS::RetainPtr(State.ShaderCompiler->newRenderPipelineState(pipeDesc, State.ShaderCompOptions.get(), &error));
		if (!State.Pipeline)
		{
			NS::String* desc = error->localizedDescription();
			printf("Shader compile error: %s\n", desc->utf8String());
		}

		MTL4::RenderPipelineDescriptor* skPipeDesc = MTL4::RenderPipelineDescriptor::alloc()->init();
		skPipeDesc->setVertexFunctionDescriptor(vertSkFuncDesc);
		skPipeDesc->setFragmentFunctionDescriptor(fragFuncDesc);
		skPipeDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);
		State.SkPipeline = NS::RetainPtr(State.ShaderCompiler->newRenderPipelineState(skPipeDesc, State.ShaderCompOptions.get(), &error));
		if (!State.SkPipeline)
		{
			NS::String* desc = error->localizedDescription();
			printf("Shader compile error: %s\n", desc->utf8String());
		}
	}

	// PHYSICS
	{
		// Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
		// This needs to be done before any other Jolt function is called.
		JPH::RegisterDefaultAllocator();

		// Install trace and assert callbacks
		JPH::Trace = ph::TraceImpl;
		JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = ph::AssertFailedImpl;)

		// Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of
		// saved data. It is not directly used in this example but still required.
		JPH::Factory::sInstance = new JPH::Factory();

		// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
		// If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
		// If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will
		// create one for you.
		JPH::RegisterTypes();

		// We need a temp allocator for temporary allocations during the physics update. We're
		// pre-allocating 10 MB to avoid having to do allocations during the physics update.
		// B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
		// If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
		// malloc / free.
		State.TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

		// We need a job system that will execute physics jobs on multiple threads. Typically
		// you would implement the JobSystem interface yourself and let Jolt Physics run on top
		// of your own job scheduler. JobSystemThreadPool is an example implementation.
		State.JobSystem.Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 2);

		// This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
		// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
		const uint cMaxBodies = 1024;

		// This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
		const uint cNumBodyMutexes = 0;

		// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
		// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
		// too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
		// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
		const uint cMaxBodyPairs = 1024;

		// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
		// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
		// Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
		const uint cMaxContactConstraints = 1024;

		// Create mapping table from object layer to broadphase layer
		// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
		// Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
		// ph::BPLayerInterfaceImpl broad_phase_layer_interface;

		// Create class that filters object vs broadphase layers
		// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
		// Also have a look at ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for a simpler interface.
		// ph::ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;

		// Create class that filters object vs object layers
		// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
		// Also have a look at ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler interface.
		// ph::ObjectLayerPairFilterImpl object_vs_object_layer_filter;

		// Now we can create the actual physics system.
		State.PhysicsSystem.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, State.BroadPhaseLayerInterface,
			State.ObjectVsBroadphaseLayerFilter, State.ObjectVsObjectLayerFilter);

		// A body activation listener gets notified when bodies activate and go to sleep
		// Note that this is called from a job so whatever you do here needs to be thread safe.
		// Registering one is entirely optional.
		State.PhysicsSystem.SetBodyActivationListener(&State.BodyActivationListener);

		// A contact listener gets notified when bodies (are about to) collide, and when they separate again.
		// Note that this is called from a job so whatever you do here needs to be thread safe.
		// Registering one is entirely optional.
		State.PhysicsSystem.SetContactListener(&State.ContactListener);

		// The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
		// variant of this. We're going to use the locking version (even though we're not planning to access bodies from multiple threads)
		JPH::BodyInterface& body_interface = State.PhysicsSystem.GetBodyInterface();

		// Next we can create a rigid body to serve as the floor, we make a large box
		// Create the settings for the collision volume (the shape).
		// Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
		JPH::BoxShapeSettings floor_shape_settings(JPH::Vec3(50.0f, 0.5f, 50.0f));
		floor_shape_settings.SetEmbedded(); // A ref counted object on the stack (base class RefTarget) should be marked as such to prevent it from being freed
											// when its reference count goes to 0.

		// Create the shape
		JPH::ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
		JPH::ShapeRefC floor_shape =
			floor_shape_result.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

		// Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
		JPH::BodyCreationSettings floor_settings(
			floor_shape, JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Static, ph::Layers::NON_MOVING);
		// floor_settings.mFriction = 0.99f;

		// Create the actual rigid body
		State.Floor = body_interface.CreateBody(floor_settings); // Note that if we run out of bodies this can return nullptr

		// Add it to the world
		body_interface.AddBody(State.Floor->GetID(), JPH::EActivation::DontActivate);

		// Now create a dynamic body to bounce on the floor
		// Note that this uses the shorthand version of creating and adding a body to the world
		JPH::BodyCreationSettings capsule_settings(
			new JPH::CapsuleShape(0.875f, 0.25f), JPH::RVec3(0.0f, 0.0f, -2.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, ph::Layers::MOVING);
		// Tuning for stability: friction + linear/angular damping
		// Do not override mass by default to keep Jolt's defaults sensible
		capsule_settings.mFriction = 0.9f;
		capsule_settings.mLinearDamping = 0.45f; // stronger damping to reduce jitter
		capsule_settings.mAngularDamping = 0.45f;
		State.PlayerCapsuleID = body_interface.CreateAndAddBody(capsule_settings, JPH::EActivation::Activate);
		// body_interface.SetLinearVelocity(State.PlayerCapsuleID, JPH::Vec3(0.0f, 0.0f, 0.0f));
		// body_interface.SetFriction(State.PlayerCapsuleID, State.Player->PlayerFrictionCoeficient);
		// body_interface.SetGravityFactor(State.PlayerCapsuleID, 35.0f);

		// Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's
		// pointless here because we only have 2 bodies). You should definitely not call this every frame or when e.g. streaming in a new level section as
		// it is an expensive operation. Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
		State.PhysicsSystem.OptimizeBroadPhase();
	}
}

RE_API auto GameUpdateAndRender(FGameMemory& Memory, f32 DeltaTime, FOffscreenBuffer& Buffer, const FInputGamepad& Gamepad) -> void
{
	NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

	if (DeltaTime > 1.0f / 120.f)
	{
		DeltaTime = 1.0f / 120.f;
	}

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

		State.Projection = glm::perspectiveFov(glm::radians(State.FOV), (f32)State.Width, (f32)State.Height, 0.01f, 1000.0f);
	}
	// Player movement is handled by PlayerController module
	PlayerCtrl::Update(State.PhysicsSystem, State.PlayerCapsuleID, Gamepad, DeltaTime, State.CameraYaw);

	State.PhysicsSystem.Update(DeltaTime, 4, State.TempAllocator, &State.JobSystem);

	auto& BodyInterface = State.PhysicsSystem.GetBodyInterface();
	const auto& TempPos = BodyInterface.GetPosition(State.PlayerCapsuleID);
	glm::vec3 physPos = glm::vec3(TempPos.GetX(), TempPos.GetY(), TempPos.GetZ());
	// Exponential smoothing to reduce jitter between physics updates and render
	{
		const float SmoothSpeed = 18.0f; // larger = faster to reach physics pos
		float alpha = 1.0f - expf(-SmoothSpeed * DeltaTime);
		State.Player->Position = glm::mix(State.LastRenderedPlayerPos, physPos, alpha);
		State.LastRenderedPlayerPos = State.Player->Position;
	}

	// Update player mesh rotation to face movement direction (smooth)
	{
		auto linVel = BodyInterface.GetLinearVelocity(State.PlayerCapsuleID);
		glm::vec2 velXZ = glm::vec2(linVel.GetX(), linVel.GetZ());
		float speed = glm::length(velXZ);
		const float TurnThreshold = 0.25f; // only rotate when moving sufficiently
		if (speed > TurnThreshold)
		{
			// Compute heading angle relative to +Z axis
			float heading = atan2f(velXZ.x, velXZ.y);

			// Build target quaternion (rotation around Y axis)
			glm::quat targetRot = glm::angleAxis(heading, glm::vec3(0.0f, 1.0f, 0.0f));
			glm::quat preserveRot = glm::quat(glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f));

			// Smoothly interpolate current rotation toward target
			const float RotLerp = 0.18f;
			State.Player->Rotation = glm::slerp(State.Player->Rotation, targetRot * preserveRot, RotLerp);
		}
	}

	// Make camera to follow player
	{
		// Right-stick camera rotation + smooth follow
		const float CamLerp = 0.12f;	   // smoothing factor, tweakable
		const float CamYawSpeed = 90.0f;   // degrees per second for full stick deflection
		const float CamPitchSpeed = 60.0f; // degrees per second

		// Update camera yaw/pitch from right thumbstick
		State.CameraYaw += Gamepad.GamepadThumbstickRight.X * CamYawSpeed * DeltaTime;
		State.CameraPitch += -Gamepad.GamepadThumbstickRight.Y * CamPitchSpeed * DeltaTime; // invert Y
		// clamp pitch
		if (State.CameraPitch < -10.0f)
			State.CameraPitch = -10.0f;
		if (State.CameraPitch > 60.0f)
			State.CameraPitch = 60.0f;

		// base distance and height
		const float CamDistance = 2.5f;
		const float CamHeight = 1.5f;

		float yawRad = glm::radians(State.CameraYaw);
		float pitchRad = glm::radians(State.CameraPitch);

		// spherical offset (left-handed, negative Z for behind)
		glm::vec3 offset = glm::vec3(-sinf(yawRad) * cosf(pitchRad), sinf(pitchRad), -cosf(yawRad) * cosf(pitchRad)) * CamDistance;
		offset.y = offset.y * CamDistance; // adjust vertical component by distance
		// add camera height
		offset.y += CamHeight;

		auto DesiredCam = State.Player->Position + offset + glm::vec3(-0.25f, 0.0f, 0.0f);
		// Snap camera instantly to desired position (no momentum) to avoid interpolation jitter
		State.CameraPosition = DesiredCam;
		auto LookAtPosition = State.Player->Position + glm::vec3(-0.25f, 1.25f, 0.0f);
		State.View = glm::lookAtLH(State.CameraPosition, LookAtPosition, glm::vec3(0, 1, 0));
	}

	State.Capsule->Position = State.Player->Position + glm::vec3(0.0f, 0.875f, 0.0f);
	{
		auto& Transform = State.MeshTransforms[0];
		Transform.Position = State.Player->Position;
	}

	MTL4::CommandBuffer* Cmd = State.Device->newCommandBuffer();
	Cmd->beginCommandBuffer(State.Allocator.get());
	Cmd->useResidencySet(State.ResidencySet.get());

	{
		State.ResidencySet->removeAllAllocations();
		for (auto& Tex : State.Textures)
		{
			State.ResidencySet->addAllocation(Tex);
		}
		State.ResidencySet->addAllocation(State.MaterialBuffer);
		State.ResidencySet->commit();
	}

	{
		State.DrawableRenderPass->colorAttachments()->object(0)->setTexture(Drawable->texture());
		State.DrawableRenderPass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.08, 0.18, 0.8, 1.0));
		State.DrawableRenderPass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
		State.DrawableRenderPass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

		MTL4::RenderCommandEncoder* Encoder = Cmd->renderCommandEncoder(State.DrawableRenderPass.get());
		Encoder->setRenderPipelineState(State.Pipeline.get());
		Encoder->setDepthStencilState(State.DepthStencilState);

		for (u32 Idx = 0; Idx < State.MeshRenderers.size(); Idx++)
		{
			auto& Transform = State.MeshTransforms[Idx];
			auto& Render = State.MeshRenderers[Idx];

			auto MeshUniform = static_cast<Engine::Mesh::FMeshUniform*>(Render.UniformBuffer->contents());
			MeshUniform->ViewProj = State.Projection * State.View;
			MeshUniform->Model = Engine::Transform::ToMatrix(Transform);

			Cmd->useResidencySet(Render.ResidencySet.get());
			Encoder->setArgumentTable(Render.ArgTableVert.get(), MTL::RenderStageVertex);

			for (auto& Sub : Render.Submeshes)
			{
				Sub.ArgTableFrag->setSamplerState(State.Sampler->gpuResourceID(), 0);
				Sub.ArgTableFrag->setAddress(State.MaterialBuffer->gpuAddress(), 0);
				Encoder->setArgumentTable(Sub.ArgTableFrag.get(), MTL::RenderStageFragment);

				Encoder->setTriangleFillMode(Sub.bWireframe ? MTL::TriangleFillModeLines : MTL::TriangleFillModeFill);
				Encoder->setCullMode(Sub.bDoubleSided ? MTL::CullModeNone : MTL::CullModeBack);
				Encoder->setFrontFacingWinding(Sub.bWindingOrderClockWise ? MTL::WindingClockwise : MTL::WindingCounterClockwise);
				Encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, Sub.IndexCount, Sub.IndexType, Sub.IbAddress, Sub.IbSize, 1);
			}
		}

		Encoder->setRenderPipelineState(State.SkPipeline.get());

		for (u32 Idx = 0; Idx < State.SkinnedMeshRenderers.size(); Idx++)
		{
			auto& Transform = State.SkinnedMeshTransforms[Idx];
			auto& Render = State.SkinnedMeshRenderers[Idx];

			auto MeshUniform = static_cast<Engine::Mesh::FMeshUniform*>(Render.UniformBuffer->contents());
			MeshUniform->ViewProj = State.Projection * State.View;
			MeshUniform->Model = Engine::Transform::ToMatrix(Transform);

			Cmd->useResidencySet(Render.ResidencySet.get());
			Encoder->setArgumentTable(Render.ArgTableVert.get(), MTL::RenderStageVertex);

			for (auto& Sub : Render.Submeshes)
			{
				Sub.ArgTableFrag->setSamplerState(State.Sampler->gpuResourceID(), 0);
				Sub.ArgTableFrag->setAddress(State.MaterialBuffer->gpuAddress(), 0);
				Encoder->setArgumentTable(Sub.ArgTableFrag.get(), MTL::RenderStageFragment);

				Encoder->setTriangleFillMode(Sub.bWireframe ? MTL::TriangleFillModeLines : MTL::TriangleFillModeFill);
				Encoder->setCullMode(Sub.bDoubleSided ? MTL::CullModeNone : MTL::CullModeBack);
				Encoder->setFrontFacingWinding(Sub.bWindingOrderClockWise ? MTL::WindingClockwise : MTL::WindingCounterClockwise);
				Encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, Sub.IndexCount, Sub.IndexType, Sub.IbAddress, Sub.IbSize, 1);
			}
		}

		Encoder->endEncoding();
	}

	Cmd->endCommandBuffer();
	std::vector<MTL4::CommandBuffer*> CmdBuffers = { Cmd };
	State.Queue->wait(Drawable);
	State.Queue->commit(CmdBuffers.data(), CmdBuffers.size());
	State.Queue->signalDrawable(Drawable);
	Drawable->present();

	State.Queue->signalEvent(State.PacingEvent.get(), State.PacingTimeStampIndex);

	pool->release();
}
