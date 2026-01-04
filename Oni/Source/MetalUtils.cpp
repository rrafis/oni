// Imple for the metal-cpp
#define CA_PRIVATE_IMPLEMENTATION
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTLFX_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <MetalFX/MetalFX.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "MeshUtils.h"

namespace Engine::Mesh
{
	namespace MDLIO
	{
		bool operator==(const FAttributeDesc& Lhs, const FAttributeDesc& Rhs)
		{
			return Lhs.Semantic == Rhs.Semantic && Lhs.Type == Rhs.Type && Lhs.Offset == Rhs.Offset && Lhs.Stride == Rhs.Stride
				&& Lhs.BufferIndex == Rhs.BufferIndex;
		}

		bool operator!=(const FAttributeDesc& Lhs, const FAttributeDesc& Rhs)
		{
			return !(Lhs == Rhs);
		}
	} // namespace MDLIO

	namespace Metal
	{
		inline auto MapIndexType(EIndexType Type) -> MTL::IndexType
		{
			switch (Type)
			{
				case EIndexType::UInt16:
					return MTL::IndexTypeUInt16;
				case EIndexType::UInt32:
					return MTL::IndexTypeUInt32;
				default:
					return MTL::IndexTypeUInt32;
			}
		}

		inline auto MapPrimitiveType(EPrimitiveType Type) -> MTL::PrimitiveType
		{
			switch (Type)
			{
				case EPrimitiveType::Triangles:
					return MTL::PrimitiveTypeTriangle;
				case EPrimitiveType::Lines:
					return MTL::PrimitiveTypeLine;
				default:
					return MTL::PrimitiveTypeTriangle;
			}
		}

		inline auto MapDataType(EDataType Type) -> MTL::VertexFormat
		{
			switch (Type)
			{
				case EDataType::Float1:
					return MTL::VertexFormatFloat;
				case EDataType::Float2:
					return MTL::VertexFormatFloat2;
				case EDataType::Float3:
					return MTL::VertexFormatFloat3;
				case EDataType::Float4:
					return MTL::VertexFormatFloat4;
				case EDataType::Half2:
					return MTL::VertexFormatHalf2;
				case EDataType::Half4:
					return MTL::VertexFormatHalf4;
				case EDataType::UShort1:
					return MTL::VertexFormatUShort;
				case EDataType::UShort2:
					return MTL::VertexFormatUShort2;
				case EDataType::UShort4:
					return MTL::VertexFormatUShort4;
				case EDataType::UInt1:
					return MTL::VertexFormatUInt;
				case EDataType::UInt2:
					return MTL::VertexFormatUInt2;
				case EDataType::UInt3:
					return MTL::VertexFormatUInt3;
				case EDataType::UInt4:
					return MTL::VertexFormatUInt4;
				default:
					return MTL::VertexFormatInvalid;
			}
		}

		auto ProcessMeshData(MTL::Device* Device, const MDLIO::FMeshData& MeshData) -> FMeshRenderer
		{
			FMeshRenderer MeshRender;

			MeshRender.VertexDescriptor = MTL::VertexDescriptor::alloc()->init();
			std::unordered_map<uint8_t, uint16_t> strideMap;
			u32 AttrIndex = 0;
			for (auto& Attr : MeshData.Attributes)
			{
				MeshRender.VertexDescriptor->attributes()->object(AttrIndex)->setFormat(Engine::Mesh::Metal::MapDataType(Attr.Type));
				MeshRender.VertexDescriptor->attributes()->object(AttrIndex)->setOffset(Attr.Offset);
				MeshRender.VertexDescriptor->attributes()->object(AttrIndex)->setBufferIndex(Attr.BufferIndex);

				strideMap[Attr.BufferIndex] = Attr.Stride;

				++AttrIndex;
			}
			for (auto [k, v] : strideMap)
			{
				MeshRender.VertexDescriptor->layouts()->object(k)->setStride(v);
				MeshRender.VertexDescriptor->layouts()->object(k)->setStepFunction(MTL::VertexStepFunctionPerVertex);
				MeshRender.VertexDescriptor->layouts()->object(k)->setStepRate(1);
			}

			for (auto& Stream : MeshData.Streams)
			{
				MTL::Buffer* Buffer = Device->newBuffer(Stream.Data.data(), Stream.Data.size(), MTL::ResourceStorageModeShared);
				MeshRender.VertexBuffers.emplace_back(Buffer);
				MeshRender.VertexBufferOffsets.emplace_back(0);
			}

			for (auto& Submesh : MeshData.Submeshes)
			{
				MTL::Buffer* Buffer = Device->newBuffer(Submesh.Indices.data(), Submesh.Indices.size(), MTL::ResourceStorageModeShared);

				FMeshRenderer::FSubmesh Sub;
				Sub.IndexType = Engine::Mesh::Metal::MapIndexType(Submesh.IndexType);
				Sub.IndexCount = Submesh.IndexCount;
				Sub.IndexBuffer = Buffer;
				MeshRender.Submeshes.emplace_back(Sub);
			}

			return MeshRender;
		}
	} // namespace Metal
} // namespace Engine::Mesh
