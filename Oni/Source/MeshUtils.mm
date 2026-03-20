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
                    // TODO: Load texture
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

			// 1) Vertex buffer
			assert(mtkMesh.vertexBuffers.count >= 1);
			MDLVertexDescriptor* vdesc = mtkMesh.vertexDescriptor;
			{
				MTKMeshBuffer* mbuf = mtkMesh.vertexBuffers[0];
				FVertexStream& vs = out.VertexStream;
				vs.Stride = (uint32_t)[vdesc.layouts[0] stride];
				id<MTLBuffer> mtlBuf = [(id<MTLBuffer>)mbuf.buffer retain];
				size_t len = mbuf.length;
				vs.Data.resize(len);
				std::memcpy(vs.Data.data(), ((uint8_t*)[mtlBuf contents]) + mbuf.offset, len);
				[mtlBuf release];
			}

			// 2) Materials: build table and reference indices
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

			// 3) Submeshes and indices
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

		// Export a single MTKMesh/MDLMesh pair to FMeshData
		static FSkeletalMeshData ExportSkeletalMesh(id<MTLDevice> device, MDLMesh* mdlMesh, MDLSkeleton* skeleton)
		{
			FSkeletalMeshData out(ExportMesh(device, mdlMesh));

			MTKMesh* mtkMesh = [[MTKMesh alloc] initWithMesh:mdlMesh device:device error:nil];
			
			// 1) Vertex skin buffer
			assert(mtkMesh.vertexBuffers.count == 2);
			MDLVertexDescriptor* vdesc = mtkMesh.vertexDescriptor;
			{
				MTKMeshBuffer* mbuf = mtkMesh.vertexBuffers[1];
				FVertexStream& vs = out.SkinVertexStream;
				vs.Stride = (uint32_t)[vdesc.layouts[1] stride];
				id<MTLBuffer> mtlBuf = [(id<MTLBuffer>)mbuf.buffer retain];
				size_t len = mbuf.length;
				vs.Data.resize(len);
				std::memcpy(vs.Data.data(), ((uint8_t*)[mtlBuf contents]) + mbuf.offset, len);
				[mtlBuf release];
			}

			[mtkMesh release];

			return out;
		}

		auto GetStaticMeshVertexDescriptor() -> MDLVertexDescriptor*
		{
			auto vertexDescriptor = [[MDLVertexDescriptor alloc] init];
			// Position
			vertexDescriptor.attributes[0] = [[MDLVertexAttribute alloc] initWithName:MDLVertexAttributePosition
																				format:MDLVertexFormatFloat3
																				offset:0
																		bufferIndex:0];
			// Normal
			vertexDescriptor.attributes[1] = [[MDLVertexAttribute alloc] initWithName:MDLVertexAttributeNormal
																				format:MDLVertexFormatFloat3
																				offset:12
																		bufferIndex:0];
			// Tangent
			vertexDescriptor.attributes[2] = [[MDLVertexAttribute alloc] initWithName:MDLVertexAttributeTangent
																				format:MDLVertexFormatFloat3
																				offset:24
																		bufferIndex:0];
			// UV
			vertexDescriptor.attributes[3] = [[MDLVertexAttribute alloc] initWithName:MDLVertexAttributeTextureCoordinate
																				format:MDLVertexFormatFloat2
																				offset:36
																		bufferIndex:0];
			// Stride = size of your vertex struct (e.g., 44 bytes)
			vertexDescriptor.layouts[0] = [[MDLVertexBufferLayout alloc] initWithStride:44];
			[vertexDescriptor setPackedOffsets];
			return vertexDescriptor;
		}

		auto GetSkeletalMeshVertexDescriptor() -> MDLVertexDescriptor*
		{
			auto vertexDescriptor = GetStaticMeshVertexDescriptor();
			// Joints
			vertexDescriptor.attributes[4] = [[MDLVertexAttribute alloc] initWithName:MDLVertexAttributeJointIndices
																				format:MDLVertexFormatUInt4
																				offset:0
																		bufferIndex:1];
			// Weights
			vertexDescriptor.attributes[5] = [[MDLVertexAttribute alloc] initWithName:MDLVertexAttributeJointWeights
																				format:MDLVertexFormatFloat4
																				offset:16
																		bufferIndex:1];
			vertexDescriptor.layouts[1] = [[MDLVertexBufferLayout alloc] initWithStride:32];
			[vertexDescriptor setPackedOffsets];
			return vertexDescriptor;
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
				mdlMesh.vertexDescriptor = GetStaticMeshVertexDescriptor();

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
				mdlMesh.vertexDescriptor = GetStaticMeshVertexDescriptor();

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
				mdlMesh.vertexDescriptor = GetStaticMeshVertexDescriptor();

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

				auto vertexDescriptor = GetStaticMeshVertexDescriptor();
				// Load the asset
				MDLAsset* asset = [[MDLAsset alloc] initWithURL:url vertexDescriptor:vertexDescriptor bufferAllocator:allocator];
				MDLMesh* mdlMesh = (MDLMesh*)[asset objectAtIndex:0];

				// Generate tangents for PBR normal mapping
				[mdlMesh addNormalsWithAttributeNamed:MDLVertexAttributeNormal
										creaseThreshold:0.0];
				[mdlMesh addTangentBasisForTextureCoordinateAttributeNamed:MDLVertexAttributeTextureCoordinate
														normalAttributeNamed:MDLVertexAttributeNormal
														tangentAttributeNamed:MDLVertexAttributeTangent];
				mdlMesh.vertexDescriptor = vertexDescriptor;

				auto Mesh = ExportMesh((id<MTLDevice>)Device, mdlMesh);

				return Mesh;
			}
		}

		void CollectMeshesAndSkeletonFromObject(MDLObject* object, NSMutableArray<MDLMesh*>* meshes, MDLSkeleton * __autoreleasing * skeleton)
		{
			// If this object is a mesh, add it
			if ([object isKindOfClass:[MDLMesh class]]) {
				[meshes addObject:(MDLMesh *)object];
			}

			// If this object is a skeleton, capture it
			if ([object isKindOfClass:[MDLSkeleton class]]) {
				*skeleton = (MDLSkeleton *)object;
			}

			// Recurse into children
			for (MDLObject *child in object.children) {
				CollectMeshesAndSkeletonFromObject(child, meshes, skeleton);
			}
		}

		auto ExportSkeletalMeshFromPath(MTL::Device* Device, const char* Path) -> FSkMeshData
		{
			@autoreleasepool
			{
				auto allocator = [[MTKMeshBufferAllocator alloc] initWithDevice:(id<MTLDevice>)Device];

				NSString* nsPath = [NSString stringWithUTF8String:Path];
				NSURL* url = [NSURL fileURLWithPath:nsPath];

				auto vertexDescriptor = GetSkeletalMeshVertexDescriptor();
				// Load the asset
				MDLAsset* asset = [[MDLAsset alloc] initWithURL:url vertexDescriptor:vertexDescriptor bufferAllocator:allocator];
                [asset loadTextures];

				NSMutableArray<MDLMesh *> *meshes = [NSMutableArray array];
				MDLSkeleton *skeleton = nil;

				for (MDLObject *object in asset) {
					CollectMeshesAndSkeletonFromObject(object, meshes, &skeleton);
				}
				
				FSkMeshData Result{};

				for (u32 idx = 0; idx < meshes.count; idx++)
				{
					MDLMesh* mdlMesh = meshes[idx];
					// Generate tangents for PBR normal mapping
					[mdlMesh addNormalsWithAttributeNamed:MDLVertexAttributeNormal
											creaseThreshold:0.0];
					[mdlMesh addTangentBasisForTextureCoordinateAttributeNamed:MDLVertexAttributeTextureCoordinate
															normalAttributeNamed:MDLVertexAttributeNormal
															tangentAttributeNamed:MDLVertexAttributeTangent];
					mdlMesh.vertexDescriptor = vertexDescriptor;

					Result.Meshes.emplace_back(ExportSkeletalMesh((id<MTLDevice>)Device, mdlMesh, skeleton));
				}

				
				Result.RestTransforms.reserve([skeleton.jointPaths count]); // Pre-allocate memory for efficiency
				Result.BindTransforms.reserve([skeleton.jointPaths count]); // Pre-allocate memory for efficiency
				Result.JointNames.reserve([skeleton.jointPaths count]); // Pre-allocate memory for efficiency
                std::vector<matrix_float4x4> bindTransforms([skeleton.jointPaths count]);
                std::vector<matrix_float4x4> restTransforms([skeleton.jointPaths count]);
                bindTransforms.resize([skeleton.jointPaths count]);
                restTransforms.resize([skeleton.jointPaths count]);
                
                [skeleton.jointBindTransforms getFloat4x4Array:bindTransforms.data() maxCount:bindTransforms.size()];
                [skeleton.jointRestTransforms getFloat4x4Array:restTransforms.data() maxCount:restTransforms.size()];

				for (NSUInteger i = 0; i < [skeleton.jointBindTransforms elementCount]; ++i)
				{
					auto& bind = Result.BindTransforms.emplace_back();
                    auto& rest = Result.RestTransforms.emplace_back();
					for (int col = 0; col < 4; ++col)
						for (int row = 0; row < 4; ++row)
						{
                            bind[col][row] = bindTransforms[i].columns[col][row];
                            rest[col][row] = restTransforms[i].columns[col][row];
						}
				}

                // Import joint names
                for (NSString *str in skeleton.jointPaths)
                    if (str != nil)
                        Result.JointNames.emplace_back([str UTF8String]);

				return Result;
			}
		}
	}
}
