#include "MeshUtils.h"

#import <MetalKit/MetalKit.h>
#import <ModelIO/ModelIO.h>

namespace Engine
{
	auto CreateSphere(MTL::Device* Device, f32 Radius) -> FMesh
	{
		FMesh Mesh;
		MTKMesh* MetalMesh;

		@autoreleasepool
		{
			auto allocator = [[MTKMeshBufferAllocator alloc] initWithDevice:(id<MTLDevice>)Device];
			auto mdlMesh = [[MDLMesh alloc] initSphereWithExtent:{2.0f * Radius, 2.0f * Radius, 2.0f * Radius} segments:{36, 36} inwardNormals:false geometryType:MDLGeometryTypeTriangles allocator:allocator];
			
			MetalMesh = [[MTKMesh alloc] initWithMesh:mdlMesh device:(id<MTLDevice>)Device error:nil];
			[MetalMesh retain];
		}

		auto submesh = MetalMesh.submeshes[0];

		Mesh.VertexDescriptor = (MTL::VertexDescriptor*)MTKMetalVertexDescriptorFromModelIO([MetalMesh vertexDescriptor]);
		Mesh.VertexBuffer = (MTL::Buffer*)MetalMesh.vertexBuffers[0].buffer;
		Mesh.IndexCount = submesh.indexCount;
		Mesh.IndexType = (MTL::IndexType)submesh.indexType;
		Mesh.IndexBuffer = (MTL::Buffer*)submesh.indexBuffer.buffer;
		Mesh.MetalMesh = (void*)MetalMesh;

		return Mesh;
	}

	auto FreeMesh(FMesh Mesh) -> void
	{
		if (Mesh.MetalMesh)
		{
			auto MetalMesh = (MTKMesh*)Mesh.MetalMesh;
			[MetalMesh release];
		}
	}
}