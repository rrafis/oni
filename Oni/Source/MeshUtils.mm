#include "MeshUtils.h"

#import <MetalKit/MetalKit.h>
#import <ModelIO/ModelIO.h>

#include <unordered_map>
#include <string>
#include <vector>
#include <cstring>

namespace Engine::Mesh
{
	// Helpers: map MDL vertex formats and semantics to engine enums
	static EDataType MapMDLFormatToDataType(MDLVertexFormat fmt)
	{
		switch (fmt)
		{
			case MDLVertexFormatFloat:
				return EDataType::Float1;
			case MDLVertexFormatFloat2:
				return EDataType::Float2;
			case MDLVertexFormatFloat3:
				return EDataType::Float3;
			case MDLVertexFormatFloat4:
				return EDataType::Float4;
			case MDLVertexFormatHalf2:
				return EDataType::Half2;
			case MDLVertexFormatHalf4:
				return EDataType::Half4;
			case MDLVertexFormatUShort:
				return EDataType::UShort1;
			case MDLVertexFormatUShort2:
				return EDataType::UShort2;
			case MDLVertexFormatUShort4:
				return EDataType::UShort4;
			case MDLVertexFormatUInt:
				return EDataType::UInt1;
			case MDLVertexFormatUInt2:
				return EDataType::UInt2;
			case MDLVertexFormatUInt3:
				return EDataType::UInt3;
			case MDLVertexFormatUInt4:
				return EDataType::UInt4;
			default:
				return EDataType::Invalid; // sensible default; adjust as needed
		}
	}

	static ESemantic MapMDLSemanticToSemantic(NSString* name)
	{
		if ([name isEqualToString:MDLVertexAttributePosition])
			return ESemantic::Position;
		if ([name isEqualToString:MDLVertexAttributeNormal])
			return ESemantic::Normal;
		if ([name isEqualToString:MDLVertexAttributeTangent])
			return ESemantic::Tangent;
		if ([name isEqualToString:MDLVertexAttributeBitangent])
			return ESemantic::Bitangent;
		if ([name isEqualToString:MDLVertexAttributeTextureCoordinate])
			return ESemantic::TexCoord0;
		if ([name isEqualToString:MDLVertexAttributeColor])
			return ESemantic::Color0;
		if ([name isEqualToString:MDLVertexAttributeJointIndices])
			return ESemantic::JointIndices;
		if ([name isEqualToString:MDLVertexAttributeJointWeights])
			return ESemantic::JointWeights;
		// Fallback: treat unknown as TexCoord1 or ignore
		return ESemantic::Invalid;
	}

	static EPrimitiveType MapMTLPrimitiveToEngine(MTLPrimitiveType prim)
	{
		switch (prim)
		{
			case MTLPrimitiveTypeTriangle:
				return EPrimitiveType::Triangles;
			case MTLPrimitiveTypeLine:
				return EPrimitiveType::Lines;
			default:
				return EPrimitiveType::Triangles;
		}
	}

	static EIndexType MapIndexType(MTKSubmesh* sm)
	{
		switch (sm.indexType)
		{
			case MTLIndexTypeUInt16:
				return EIndexType::UInt16;
			case MTLIndexTypeUInt32:
				return EIndexType::UInt32;
			default:
				return EIndexType::UInt32;
		}
	}

	namespace MDLIO
	{
		// Read a Model I/O material into PBR scaffolding
		static FPBRMaterial ExtractPBRMaterial(MDLMaterial* mdlMat)
		{
			FPBRMaterial mat{};
			if (!mdlMat)
				return mat;
			mat.Name = mdlMat.name ? std::string([mdlMat.name UTF8String]) : std::string();

			// auto getFloat = [&](NSString* Semantic, float* out, int count) {
			// 	MDLMaterialSemantic semValue = [Semantic integerValue];
			// 	MDLMaterialProperty* p = [mdlMat propertyWithSemantic:semValue];
			// 	if (!p)
			// 		return;
			// 	if (p.type == MDLMaterialPropertyTypeFloat)
			// 	{
			// 		out[0] = p.floatValue;
			// 	}
			// 	else if (p.type == MDLMaterialPropertyTypeFloat3)
			// 	{
			// 		vector_float3 v = p.float3Value;
			// 		for (int i = 0; i < std::min(count, 3); ++i)
			// 			out[i] = ((float*)&v)[i];
			// 	}
			// 	else if (p.type == MDLMaterialPropertyTypeFloat4)
			// 	{
			// 		vector_float4 v = p.float4Value;
			// 		for (int i = 0; i < std::min(count, 4); ++i)
			// 			out[i] = ((float*)&v)[i];
			// 	}
			// };

			auto getTexPath = [&](MDLMaterialSemantic sem) -> std::string {
				MDLMaterialProperty* p = [mdlMat propertyWithSemantic:sem];
				if (!p)
					return {};
				if (p.type == MDLMaterialPropertyTypeString && p.stringValue)
				{
					return std::string([p.stringValue UTF8String]);
				}
				if (p.type == MDLMaterialPropertyTypeTexture && p.textureSamplerValue.texture.name)
				{
					return std::string([p.textureSamplerValue.texture.name UTF8String]); // or resolve to file path
				}
				return {};
			};

			// BaseColor, Metallic/Roughness, Normal, Emissive, Occlusion
			{
				MDLMaterialProperty* p = [mdlMat propertyWithSemantic:MDLMaterialSemanticBaseColor];
				if (p)
				{
					if (p.type == MDLMaterialPropertyTypeFloat4)
					{
						vector_float4 v = p.float4Value;
						mat.BaseColorFactor[0] = v.x;
						mat.BaseColorFactor[1] = v.y;
						mat.BaseColorFactor[2] = v.z;
						mat.BaseColorFactor[3] = v.w;
					}
					else if (p.type == MDLMaterialPropertyTypeFloat3)
					{
						vector_float3 v = p.float3Value;
						mat.BaseColorFactor[0] = v.x;
						mat.BaseColorFactor[1] = v.y;
						mat.BaseColorFactor[2] = v.z;
					}
					mat.BaseColorTex = getTexPath(MDLMaterialSemanticBaseColor);
				}
			}
			{
				MDLMaterialProperty* m = [mdlMat propertyWithSemantic:MDLMaterialSemanticMetallic];
				if (m && m.type == MDLMaterialPropertyTypeFloat)
					mat.MetallicFactor = m.floatValue;
				MDLMaterialProperty* r = [mdlMat propertyWithSemantic:MDLMaterialSemanticRoughness];
				if (r && r.type == MDLMaterialPropertyTypeFloat)
					mat.RoughnessFactor = r.floatValue;
				// Single ORM texture if present
				std::string orm = getTexPath(MDLMaterialSemanticMetallic);
				if (orm.empty())
					orm = getTexPath(MDLMaterialSemanticRoughness);
				mat.MetallicRoughnessTex = orm;
			}
			{
				MDLMaterialProperty* e = [mdlMat propertyWithSemantic:MDLMaterialSemanticEmission];
				if (e && e.type == MDLMaterialPropertyTypeFloat3)
				{
					vector_float3 v = e.float3Value;
					mat.EmissiveFactor[0] = v.x;
					mat.EmissiveFactor[1] = v.y;
					mat.EmissiveFactor[2] = v.z;
				}
				mat.EmissiveTex = getTexPath(MDLMaterialSemanticEmission);
			}
			mat.NormalTex = getTexPath(MDLMaterialSemanticTangentSpaceNormal);
			mat.OcclusionTex = getTexPath(MDLMaterialSemanticAmbientOcclusion);
			// Double-sided
			MDLMaterialProperty* ds = [mdlMat propertyWithSemantic:MDLMaterialSemanticOpacity];
			mat.DoubleSided = ds ? ds.floatValue < 1.0f : false;

			return mat;
		}

		// Export a single MTKMesh/MDLMesh pair to FMeshData
		static FMeshData ExportMesh(id<MTLDevice> device, MDLMesh* mdlMesh)
		{
			MTKMesh* mtkMesh = [[MTKMesh alloc] initWithMesh:mdlMesh device:device error:nil];

			FMeshData out{};
			out.Name = mdlMesh.name ? std::string([mdlMesh.name UTF8String]) : std::string();
			out.VertexCount = (uint32_t)mtkMesh.vertexCount;

			// 1) Vertex descriptor -> FAttributeDesc
			MDLVertexDescriptor* vdesc = mtkMesh.vertexDescriptor;
			for (MDLVertexAttribute* attr in vdesc.attributes)
			{
				if (!attr || attr.bufferIndex == NSNotFound)
					continue;
				FAttributeDesc A{};
				A.Semantic = MapMDLSemanticToSemantic(attr.name);
				A.Type = MapMDLFormatToDataType(attr.format);
				A.Offset = (uint16_t)attr.offset;
				A.Stride = (uint16_t)[vdesc.layouts[attr.bufferIndex] stride];
				A.BufferIndex = (uint8_t)attr.bufferIndex;
				if (A.Semantic != ESemantic::Invalid && A.Type != EDataType::Invalid)
				{
					out.Attributes.push_back(A);
				}
			}

			// 2) Vertex buffers
			out.Streams.resize(mtkMesh.vertexBuffers.count);
			for (NSUInteger i = 0; i < mtkMesh.vertexBuffers.count; ++i)
			{
				MTKMeshBuffer* mbuf = mtkMesh.vertexBuffers[i];
				FVertexStream& vs = out.Streams[i];
				vs.Stride = (uint32_t)[vdesc.layouts[i] stride];
				id<MTLBuffer> mtlBuf = [(id<MTLBuffer>)mbuf.buffer retain];
				size_t len = mbuf.length;
				vs.Data.resize(len);
				std::memcpy(vs.Data.data(), ((uint8_t*)[mtlBuf contents]) + mbuf.offset, len);
				[mtlBuf release];
			}

			// 3) Materials: build table and reference indices
			// Build a material map to dedupe
			std::unordered_map<MDLMaterial*, uint32_t> matIndex;
			for (MDLSubmesh* sm in mdlMesh.submeshes)
			{
				MDLMaterial* mdlMat = sm.material;
				if (mdlMat && matIndex.find(mdlMat) == matIndex.end())
				{
					uint32_t idx = (uint32_t)out.Materials.size();
					out.Materials.push_back(ExtractPBRMaterial(mdlMat));
					matIndex[mdlMat] = idx;
				}
			}

			// 4) Submeshes and indices
			for (NSUInteger i = 0; i < mtkMesh.submeshes.count; ++i)
			{
				MTKSubmesh* msm = mtkMesh.submeshes[i];
				MDLSubmesh* dsm = mdlMesh.submeshes[i];
				FSubmeshDesc S{};
				S.IndexCount = (uint32_t)msm.indexCount;
				S.IndexType = MapIndexType(msm);
				S.PrimitiveType = MapMTLPrimitiveToEngine(msm.primitiveType);
				S.MaterialIndex = 0;
				if (dsm.material && matIndex.find(dsm.material) != matIndex.end())
				{
					S.MaterialIndex = matIndex[dsm.material];
				}

				// Copy index data
				id<MTLBuffer> idxBuf = (id<MTLBuffer>)msm.indexBuffer.buffer;
				size_t len = msm.indexBuffer.length;
				S.Indices.resize(len);
				std::memcpy(S.Indices.data(), ((uint8_t*)[idxBuf contents]) + msm.indexBuffer.offset, len);

				out.Submeshes.push_back(std::move(S));
			}

			[mtkMesh release];

			return out;
		}

		auto ExportSphere(MTL::Device* Device, f32 Radius, u32 Segmentes) -> FMeshData
		{
			@autoreleasepool
			{
				auto allocator = [[MTKMeshBufferAllocator alloc] initWithDevice:(id<MTLDevice>)Device];
				MDLMesh* mdlMesh = [[MDLMesh alloc] initSphereWithExtent:{Radius*2.0f, Radius*2.0f, Radius*2.0f}
														        segments:{Segmentes,Segmentes}
														   inwardNormals:NO
														    geometryType:MDLGeometryTypeTriangles
														       allocator:allocator];

				// Generate tangents for PBR normal mapping
				[mdlMesh addNormalsWithAttributeNamed:MDLVertexAttributeNormal
										creaseThreshold:0.0];
				[mdlMesh addTangentBasisForTextureCoordinateAttributeNamed:MDLVertexAttributeTextureCoordinate
														normalAttributeNamed:MDLVertexAttributeNormal
														tangentAttributeNamed:MDLVertexAttributeTangent];

				NSError* err = nil;
				// Assign a simple material
				MDLMaterial* mat = [[MDLMaterial alloc] initWithName:@"SphereMat"
													scatteringFunction:[[MDLScatteringFunction alloc] init]];
				MDLMaterialProperty* baseColor = [[MDLMaterialProperty alloc] initWithName:@"baseColor"
																					semantic:MDLMaterialSemanticBaseColor float4:{1,1,1,1}];
				[mat setProperty:baseColor];

				for (MDLSubmesh* sm in mdlMesh.submeshes)
				{
					sm.material = mat;
				}

				auto Mesh = ExportMesh((id<MTLDevice>)Device, mdlMesh);

				return Mesh;
			}
		}

		auto ExportBox(MTL::Device* Device, glm::vec3 Extent, glm::uvec3 Segmentes) -> FMeshData
		{
			@autoreleasepool
			{
				auto allocator = [[MTKMeshBufferAllocator alloc] initWithDevice:(id<MTLDevice>)Device];
				MDLMesh* mdlMesh = [[MDLMesh alloc] initBoxWithExtent:{Extent.x, Extent.y, Extent.z} 
														     segments:{Segmentes.x, Segmentes.y, Segmentes.z}
														inwardNormals:NO
														 geometryType:MDLGeometryTypeTriangles
														    allocator:allocator];

				// Generate tangents for PBR normal mapping
				[mdlMesh addNormalsWithAttributeNamed:MDLVertexAttributeNormal
										creaseThreshold:0.0];
				[mdlMesh addTangentBasisForTextureCoordinateAttributeNamed:MDLVertexAttributeTextureCoordinate
														normalAttributeNamed:MDLVertexAttributeNormal
														tangentAttributeNamed:MDLVertexAttributeTangent];

				NSError* err = nil;
				// Assign a simple material
				MDLMaterial* mat = [[MDLMaterial alloc] initWithName:@"SphereMat"
													scatteringFunction:[[MDLScatteringFunction alloc] init]];
				MDLMaterialProperty* baseColor = [[MDLMaterialProperty alloc] initWithName:@"baseColor"
																					semantic:MDLMaterialSemanticBaseColor float4:{1,1,1,1}];
				[mat setProperty:baseColor];

				for (MDLSubmesh* sm in mdlMesh.submeshes)
				{
					sm.material = mat;
				}

				auto Mesh = ExportMesh((id<MTLDevice>)Device, mdlMesh);

				return Mesh;
			}
		}

		auto ExportCapsule(MTL::Device* Device, glm::vec3 Extent, glm::uvec3 Segmentes) -> FMeshData
		{
			@autoreleasepool
			{
				auto allocator = [[MTKMeshBufferAllocator alloc] initWithDevice:(id<MTLDevice>)Device];
				MDLMesh* mdlMesh = [[MDLMesh alloc] initCapsuleWithExtent:{Extent.x, Extent.y, Extent.z} 
														 cylinderSegments:{Segmentes.x, Segmentes.y}
													   hemisphereSegments:Segmentes.z
															inwardNormals:NO
															 geometryType:MDLGeometryTypeTriangles
															    allocator:allocator];

				// Generate tangents for PBR normal mapping
				[mdlMesh addNormalsWithAttributeNamed:MDLVertexAttributeNormal
										creaseThreshold:0.0];
				[mdlMesh addTangentBasisForTextureCoordinateAttributeNamed:MDLVertexAttributeTextureCoordinate
														normalAttributeNamed:MDLVertexAttributeNormal
														tangentAttributeNamed:MDLVertexAttributeTangent];

				NSError* err = nil;
				// Assign a simple material
				MDLMaterial* mat = [[MDLMaterial alloc] initWithName:@"SphereMat"
													scatteringFunction:[[MDLScatteringFunction alloc] init]];
				MDLMaterialProperty* baseColor = [[MDLMaterialProperty alloc] initWithName:@"baseColor"
																					semantic:MDLMaterialSemanticBaseColor float4:{1,1,1,1}];
				[mat setProperty:baseColor];

				for (MDLSubmesh* sm in mdlMesh.submeshes)
				{
					sm.material = mat;
				}

				auto Mesh = ExportMesh((id<MTLDevice>)Device, mdlMesh);

				return Mesh;
			}
		}

		auto ExportMeshFromPath(MTL::Device* Device, const char* Path) -> FMeshData
		{
			@autoreleasepool
			{
				auto allocator = [[MTKMeshBufferAllocator alloc] initWithDevice:(id<MTLDevice>)Device];

				NSString* nsPath = [NSString stringWithUTF8String:Path];
				NSURL* url = [NSURL fileURLWithPath:nsPath];

				// Load the asset
				MDLAsset* asset = [[MDLAsset alloc] initWithURL:url vertexDescriptor:nil bufferAllocator:allocator];
				MDLMesh* mdlMesh = (MDLMesh*)[asset objectAtIndex:0];

				// Generate tangents for PBR normal mapping
				[mdlMesh addNormalsWithAttributeNamed:MDLVertexAttributeNormal
										creaseThreshold:0.0];
				[mdlMesh addTangentBasisForTextureCoordinateAttributeNamed:MDLVertexAttributeTextureCoordinate
														normalAttributeNamed:MDLVertexAttributeNormal
														tangentAttributeNamed:MDLVertexAttributeTangent];

				auto Mesh = ExportMesh((id<MTLDevice>)Device, mdlMesh);

				return Mesh;
			}
		}
	}
}