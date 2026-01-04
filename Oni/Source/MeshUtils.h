#pragma once

#include <Platform.h>

#include <Metal/Metal.hpp>

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

	struct FMeshRenderer
	{
		struct FSubmesh
		{
			MTL::IndexType IndexType;
			u32 IndexCount;
			MTL::Buffer* IndexBuffer;
		};

		MTL::VertexDescriptor* VertexDescriptor;
		std::vector<MTL::Buffer*> VertexBuffers;
		std::vector<u32> VertexBufferOffsets;
		std::vector<FSubmesh> Submeshes;
	};

	namespace MDLIO
	{
		struct FAttributeDesc
		{
			ESemantic Semantic;
			EDataType Type;
			u16 Offset;		// byte offset in the stream
			u16 Stride;		// byte stride of the stream
			u8 BufferIndex; // which vertex stream
		};
		bool operator==(const FAttributeDesc& Lhs, const FAttributeDesc& Rhs);
		bool operator!=(const FAttributeDesc& Lhs, const FAttributeDesc& Rhs);

		struct FVertexStream
		{
			u32 Stride;
			std::vector<u8> Data;
		};

		// Minimal PBR scaffolding
		struct FPBRMaterial
		{
			// Scalar factors
			float BaseColorFactor[4] = { 1, 1, 1, 1 };
			float MetallicFactor = 1.0f;
			float RoughnessFactor = 1.0f;
			float EmissiveFactor[3] = { 0, 0, 0 };
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
			float BoundingSphere[4]; // xyz center + radius
									 // Or AABB: min[3], max[3]
		};

		struct FMeshData
		{
			u32 VertexCount = 0;
			std::vector<FAttributeDesc> Attributes;
			std::vector<FVertexStream> Streams;
			std::vector<FSubmeshDesc> Submeshes;
			std::vector<FPBRMaterial> Materials;
			std::string Name;
		};

		auto ExportSphere(MTL::Device* Device, f32 Radius, u32 Segmentes) -> FMeshData;
		auto ExportMeshFromPath(MTL::Device* Device, const char* Path) -> FMeshData;
	} // namespace MDLIO

	namespace Metal
	{
		auto ProcessMeshData(MTL::Device* Device, const struct MDLIO::FMeshData& MeshData) -> FMeshRenderer;
	} // namespace Metal
} // namespace Engine::Mesh
