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

		auto ProcessMeshData(MTL::Device* Device, const MDLIO::FMeshData& MeshData, u32 MaterialIndexOffset) -> FMeshRenderer
		{
			FMeshRenderer MeshRender;
			MeshRender.VertexBuffer =
				NS::TransferPtr(Device->newBuffer(MeshData.VertexStream.Data.data(), MeshData.VertexStream.Data.size(), MTL::ResourceStorageModeShared));
			MeshRender.UniformBuffer = NS::TransferPtr(Device->newBuffer(sizeof(Engine::Mesh::FMeshUniform), MTL::ResourceStorageModeShared));
			MeshRender.MaterialIndexBuffer = NS::TransferPtr(Device->newBuffer(sizeof(u32) * MeshData.Submeshes.size(), MTL::ResourceStorageModeShared));

			NS::Error* error;
			NS::SharedPtr<MTL::ResidencySetDescriptor> ResSetDesc = NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());

			ResSetDesc->setInitialCapacity(1 + 1 + MeshRender.Submeshes.size()); // Vertex buffer + uniform buffer + Index buffer for each submesh
			MeshRender.ResidencySet = NS::TransferPtr(Device->newResidencySet(ResSetDesc.get(), &error));
			if (!MeshRender.ResidencySet)
			{
				NS::String* desc = error->localizedDescription();
				printf("Shader compile error: %s\n", desc->utf8String());
			}

			NS::SharedPtr<MTL4::ArgumentTableDescriptor> ArgTableVertDesc = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
			ArgTableVertDesc->setMaxBufferBindCount(11);
			ArgTableVertDesc->setMaxTextureBindCount(0);
			ArgTableVertDesc->setMaxSamplerStateBindCount(0);
			MeshRender.ArgTableVert = NS::TransferPtr(Device->newArgumentTable(ArgTableVertDesc.get(), &error));

			MeshRender.ArgTableVert->setAddress(MeshRender.VertexBuffer->gpuAddress(), 0);
			MeshRender.ArgTableVert->setAddress(MeshRender.UniformBuffer->gpuAddress(), 10);
			MeshRender.ResidencySet->addAllocation(MeshRender.VertexBuffer.get());
			MeshRender.ResidencySet->addAllocation(MeshRender.UniformBuffer.get());
			MeshRender.ResidencySet->addAllocation(MeshRender.MaterialIndexBuffer.get());

			NS::SharedPtr<MTL4::ArgumentTableDescriptor> ArgTableFragDesc = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
			ArgTableFragDesc->setMaxBufferBindCount(14);
			ArgTableFragDesc->setMaxTextureBindCount(0);
			ArgTableFragDesc->setMaxSamplerStateBindCount(1);

			u32 Idx = 0;
			u32* MatIndex = static_cast<u32*>(MeshRender.MaterialIndexBuffer->contents());
			for (auto& Submesh : MeshData.Submeshes)
			{
				MatIndex[Idx] = Submesh.MaterialIndex + MaterialIndexOffset;

				FMeshRenderer::FSubmesh Sub;
				Sub.IndexType = Engine::Mesh::Metal::MapIndexType(Submesh.IndexType);
				Sub.IndexCount = Submesh.IndexCount;
				Sub.IndexBuffer = NS::TransferPtr(Device->newBuffer(Submesh.Indices.data(), Submesh.Indices.size(), MTL::ResourceStorageModeShared));
				Sub.bDoubleSided = true; // MeshData.Materials[Sub.MaterialIndex].DoubleSided;
				Sub.bWindingOrderClockWise = true;

				Sub.IbSize = Sub.IndexCount * (sizeof(u16) * (u32)(Sub.IndexType + 1));
				Sub.IbAddress = Sub.IndexBuffer->gpuAddress();

				Sub.ArgTableFrag = NS::TransferPtr(Device->newArgumentTable(ArgTableFragDesc.get(), &error));
				Sub.ArgTableFrag->setAddress(MeshRender.MaterialIndexBuffer->gpuAddress() + Idx * sizeof(u32), 13);
				MeshRender.ResidencySet->addAllocation(Sub.IndexBuffer.get());

				MeshRender.Submeshes.emplace_back(Sub);

				Idx++;
			}
			MeshRender.ResidencySet->commit();

			return MeshRender;
		}
	} // namespace Metal
} // namespace Engine::Mesh
