#pragma once

#include <Platform.h>

#include <Metal/Metal.hpp>

namespace Engine
{
	struct FMesh
	{
		MTL::VertexDescriptor* VertexDescriptor;
		MTL::Buffer* VertexBuffer;

		u32 IndexCount;
		MTL::IndexType IndexType;
		MTL::Buffer* IndexBuffer;

		void* MetalMesh;
	};

	auto CreateSphere(MTL::Device* Device, f32 Radius) -> FMesh;
	auto FreeMesh(FMesh Mesh) -> void;
} // namespace Engine
