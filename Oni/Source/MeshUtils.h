#pragma once

#include <Platform.h>

#include <Metal/Metal.hpp>

#include <glm/glm.hpp>

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

	enum class EPrimitiveType : u8
	{
		Triangles,
		Lines
	};

	enum class EIndexType : u8
	{
		UInt16,
		UInt32
	};

	struct alignas(16) FMeshUniform
	{
		glm::mat4 ViewProj;
		glm::mat4 Model;
	};

	struct FMeshRenderer
	{
		struct FSubmesh
		{
			MTL::IndexType IndexType;
			u32 IndexCount;
			NS::SharedPtr<MTL::Buffer> IndexBuffer;
			NS::SharedPtr<MTL4::ArgumentTable> ArgTableFrag;

			u32 IbSize;
			MTL::GPUAddress IbAddress;

			bool bDoubleSided;
			bool bWindingOrderClockWise;
			bool bWireframe = false;
		};

		std::vector<FSubmesh> Submeshes;
		NS::SharedPtr<MTL::Buffer> VertexBuffer;
		NS::SharedPtr<MTL::Buffer> UniformBuffer;
		NS::SharedPtr<MTL::Buffer> MaterialIndexBuffer;
		NS::SharedPtr<MTL::ResidencySet> ResidencySet;
		NS::SharedPtr<MTL4::ArgumentTable> ArgTableVert;
	};

	struct FSkeletalMeshRenderer : public FMeshRenderer
	{
		NS::SharedPtr<MTL::Buffer> SkinBuffer;
	};

	struct FSkMeshRenderer
	{
		std::vector<FSkeletalMeshRenderer> MeshRender;
		NS::SharedPtr<MTL::Buffer> PoseTransoformBuffer;
		NS::SharedPtr<MTL::Buffer> BindTransoformBuffer;
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

	namespace MDLIO
	{
		struct FVertexStream
		{
			u32 Stride;
			std::vector<u8> Data;
		};

		// Minimal PBR scaffolding
		struct FPBRMaterial
		{
			// Scalar factors
			glm::vec4 BaseColorFactor = { 1, 1, 1, 1 };
			f32 MetallicFactor = 1.0f;
			f32 RoughnessFactor = 1.0f;
			glm::vec4 EmissiveFactor = { 0, 0, 0, 0 };
			// Texture references (store paths or IDs; resolve externally)
			std::string BaseColorTex;
			std::string MetallicRoughnessTex; // ORM or separate maps
			std::string NormalTex;
			std::string EmissiveTex;
			std::string OcclusionTex;
			// Alpha, double-sided, etc. as needed
			bool DoubleSided = false;
			std::string Name;
		};

		struct FSubmeshDesc
		{
			u32 IndexCount;
			EIndexType IndexType;
			EPrimitiveType PrimitiveType;
			u32 MaterialIndex;		 // index into materials table
			std::vector<u8> Indices; // raw index data

			// Future-proof extras
			f32 BoundingSphere[4]; // xyz center + radius
								   // Or AABB: min[3], max[3]
		};

		struct FMeshData
		{
			u32 VertexCount = 0;
			FVertexStream VertexStream;
			std::vector<FSubmeshDesc> Submeshes;
			std::vector<FPBRMaterial> Materials;
			std::string Name;
		};

		struct FSkeletalMeshData : FMeshData
		{
			FVertexStream SkinVertexStream;
		};
    
        struct FSkMeshData
        {
            std::vector<FSkeletalMeshData> Meshes;
            std::vector<glm::mat4> RestTransforms;
            std::vector<glm::mat4> BindTransforms;
            std::vector<std::string> JointNames;
        };

		auto ExportSphere(MTL::Device* Device, f32 Radius, u32 Segmentes) -> FMeshData;
		auto ExportBox(MTL::Device* Device, glm::vec3 Extent, glm::uvec3 Segmentes) -> FMeshData;
		auto ExportCapsule(MTL::Device* Device, glm::vec3 Extent, glm::uvec3 Segmentes) -> FMeshData;
		auto ExportMeshFromPath(MTL::Device* Device, const char* Path) -> FMeshData;
		auto ExportSkeletalMeshFromPath(MTL::Device* Device, const char* Path) -> FSkMeshData;
	} // namespace MDLIO

	namespace Metal
	{
		auto ProcessMeshData(MTL::Device* Device, const struct MDLIO::FMeshData& MeshData, u32 MaterialIndexOffset = 0) -> FMeshRenderer;
		auto ProcessSkeletalMeshData(MTL::Device* Device, const MDLIO::FSkMeshData& MeshData, u32 MaterialIndexOffset) -> FSkMeshRenderer;
	} // namespace Metal
} // namespace Engine::Mesh
