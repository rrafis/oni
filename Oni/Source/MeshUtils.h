#pragma once

#include "Types.h"

#include "Vfs.h"

#include <Metal/Metal.hpp>

#include <cgltf.h>

#include <filesystem>

namespace Engine::Mesh
{
	// Canonical semantics and data formats for portability
	enum class ESemantic : u8
	{
		Position,
		Normal,
		Tangent,
		Bitangent,
		TexCoord0,
		TexCoord1,
		Color0,
		JointIndices,
		JointWeights,
		Invalid,
	};

	enum class EDataType : u8
	{
		Float1,
		Float2,
		Float3,
		Float4,
		Half2,
		Half4,
		UInt1,
		UInt2,
		UInt3,
		UInt4,
		UShort1,
		UShort2,
		UShort4,
		Invalid,
	};

	struct alignas(16) FMeshUniform
	{
		glm::mat4 ViewProj;
		glm::mat4 Model;
	};

	struct alignas(16) FRenderMaterial
	{
		// Scalar factors
		glm::vec4 BaseColorFactor = { 1, 1, 1, 1 };
		glm::vec4 EmissiveFactor = { 0, 0, 0, 0 };
		f32 MetallicFactor = 1.0f;
		f32 RoughnessFactor = 1.0f;
		MTL::ResourceID BaseColorTex;
		MTL::ResourceID MetallicRoughnessTex; // ORM or separate maps
		MTL::ResourceID NormalTex;
		MTL::ResourceID EmissiveTex;
		MTL::ResourceID OcclusionTex;
	};

	namespace Metal
	{
		auto ProcessMeshData(MTL::Device* Device, const Mesh::FMeshData& MeshData, std::unordered_map<std::string, u32>& MaterialMap,
			Comp::FSkeletonRenderData* Skeleton) -> Comp::FMeshRenderer;
		auto ProcessSkeleton(MTL::Device* Device, const Mesh::FSkeleton& Skeleton) -> Comp::FSkeletonRenderData;
	} // namespace Metal

	namespace mdl
	{
		auto LoadData(MTL::Device* Device, const char* Path) -> void*;
		auto FreeData(void* Data) -> void;

		auto LoadMesh(MTL::Device* Device, void* Data) -> std::vector<Mesh::FMeshData>;
		auto LoadMaterial(MTL::Device* Device, void* Data) -> std::vector<Mesh::FMaterial>;

		auto LoadSphere(const std::string& Name, MTL::Device* Device, f32 Radius, u32 Segmentes) -> Mesh::FMeshData;
		auto LoadBox(const std::string& Name, MTL::Device* Device, glm::vec3 Extent, glm::uvec3 Segmentes) -> Mesh::FMeshData;
		auto LoadCapsule(const std::string& Name, MTL::Device* Device, glm::vec3 Extent, glm::uvec3 Segmentes) -> Mesh::FMeshData;
	} // namespace mdl

	namespace gltf
	{
		auto LoadData(const char* Path) -> cgltf_data*;
		auto FreeData(cgltf_data* Data) -> void;

		auto LoadMesh(cgltf_data* Data) -> std::vector<Mesh::FMeshData>;
		auto LoadSkeleton(cgltf_data* Data) -> std::vector<Mesh::FSkeleton>;
		auto LoadMaterial(cgltf_data* Data) -> std::vector<Mesh::FMaterial>;

		auto LoadMeshView(cgltf_data* Data) -> FMeshLoadView;
		auto ImportMesh(const FMeshLoadView&, const vfs::Path&) -> void;
	} // namespace gltf
} // namespace Engine::Mesh
