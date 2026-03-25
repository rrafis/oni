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

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <stb_image.h>
#include <stb_image_write.h>

#include <filesystem>
#include <fstream>

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
				default:
					return MTL::IndexTypeUInt32;
			}
		}

		inline auto MapIndexType(usize Stride) -> MTL::IndexType
		{
			switch (Stride)
			{
				case sizeof(u16):
					return MTL::IndexTypeUInt16;
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

		auto ProcessMeshData(MTL::Device* Device, const Mesh::FMeshData& MeshData, std::unordered_map<std::string, u32>& MaterialMap,
			Comp::FSkeletonRenderData* Skeleton) -> Comp::FMeshRenderer
		{
			Comp::FMeshRenderer MeshRender;
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
			ArgTableVertDesc->setMaxBufferBindCount(5);
			ArgTableVertDesc->setMaxTextureBindCount(0);
			ArgTableVertDesc->setMaxSamplerStateBindCount(0);
			MeshRender.ArgTableVert = NS::TransferPtr(Device->newArgumentTable(ArgTableVertDesc.get(), &error));

			MeshRender.ArgTableVert->setAddress(MeshRender.VertexBuffer->gpuAddress(), 0);
			MeshRender.ArgTableVert->setAddress(MeshRender.UniformBuffer->gpuAddress(), 1);
			MeshRender.ResidencySet->addAllocation(MeshRender.VertexBuffer.get());
			MeshRender.ResidencySet->addAllocation(MeshRender.UniformBuffer.get());
			MeshRender.ResidencySet->addAllocation(MeshRender.MaterialIndexBuffer.get());
			if (MeshData.bHasSkeleton)
			{
				MeshRender.ArgTableVert->setAddress(Skeleton->BindTransoformBuffer->gpuAddress(), 2);
				MeshRender.ArgTableVert->setAddress(Skeleton->PoseTransoformBuffer->gpuAddress(), 3);

				MeshRender.ResidencySet->addAllocation(Skeleton->BindTransoformBuffer.get());
				MeshRender.ResidencySet->addAllocation(Skeleton->PoseTransoformBuffer.get());
			}

			NS::SharedPtr<MTL4::ArgumentTableDescriptor> ArgTableFragDesc = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
			ArgTableFragDesc->setMaxBufferBindCount(2);
			ArgTableFragDesc->setMaxTextureBindCount(0);
			ArgTableFragDesc->setMaxSamplerStateBindCount(1);

			u32 Idx = 0;
			u32* MatIndex = static_cast<u32*>(MeshRender.MaterialIndexBuffer->contents());
			for (auto& Submesh : MeshData.Submeshes)
			{
				MatIndex[Idx] = MaterialMap[MeshData.Materials[Submesh.MaterialIndex]];

				Comp::FMeshRenderer::FSubmesh Sub;
				Sub.IndexType = Engine::Mesh::Metal::MapIndexType(Submesh.IndexStream.Stride);
				Sub.IndexCount = Submesh.IndexStream.Count;
				Sub.IndexBuffer =
					NS::TransferPtr(Device->newBuffer(Submesh.IndexStream.Data.data(), Submesh.IndexStream.Data.size(), MTL::ResourceStorageModeShared));
				Sub.bDoubleSided = true; // MeshData.Materials[Sub.MaterialIndex].DoubleSided;
				Sub.bWindingOrderClockWise = true;

				Sub.IbSize = Sub.IndexCount * Submesh.IndexStream.Stride;
				Sub.IbAddress = Sub.IndexBuffer->gpuAddress();

				Sub.ArgTableFrag = NS::TransferPtr(Device->newArgumentTable(ArgTableFragDesc.get(), &error));
				Sub.ArgTableFrag->setAddress(MeshRender.MaterialIndexBuffer->gpuAddress() + Idx * sizeof(u32), 1);
				MeshRender.ResidencySet->addAllocation(Sub.IndexBuffer.get());

				MeshRender.Submeshes.emplace_back(Sub);

				Idx++;
			}
			MeshRender.ResidencySet->commit();

			return MeshRender;
		}

		auto ProcessSkeleton(MTL::Device* Device, const Mesh::FSkeleton& Skeleton) -> Comp::FSkeletonRenderData
		{
			Comp::FSkeletonRenderData Result;
			auto Pose = Skeleton.RestPose.GetMatrixPalette();
			auto& Bind = Skeleton.InvBindPose;
			Result.BindTransoformBuffer = NS::TransferPtr(Device->newBuffer(Bind.data(), sizeof(glm::mat4) * Bind.size(), MTL::ResourceStorageModeShared));
			Result.PoseTransoformBuffer = NS::TransferPtr(Device->newBuffer(Pose.data(), sizeof(glm::mat4) * Pose.size(), MTL::ResourceStorageModeShared));
			return Result;
		}
	} // namespace Metal

	namespace gltf
	{
		namespace Utils
		{
			auto LoadJointNames(cgltf_data* Data) -> std::vector<std::string>
			{
				u32 BoneCount = Data->nodes_count;
				std::vector<std::string> Result(BoneCount, "Not Set");

				for (u32 Index = 0; Index < BoneCount; Index++)
				{
					cgltf_node* Node = &(Data->nodes[Index]);
					if (!Node->name)
					{
						Result[Index] = "EMPTY_NODE";
					}
					else
					{
						Result[Index] = Node->name;
					}
				}

				return Result;
			}

			auto GetScalarValues(std::vector<f32>& out, u32 compCount, const cgltf_accessor& inAccessor) -> void
			{
				out.resize(inAccessor.count * compCount);
				for (cgltf_size i = 0; i < inAccessor.count; ++i)
				{
					cgltf_accessor_read_float(&inAccessor, i, &out[i * compCount], compCount);
				}
			}

			auto GetLocalTransform(const cgltf_node& Node) -> Comp::FTransform
			{
				Comp::FTransform Result;
				if (Node.has_matrix)
				{
					glm::mat4 m;
					for (int col = 0; col < 4; ++col)
					{
						for (int row = 0; row < 4; ++row)
						{
							m[col][row] = Node.matrix[col * 4 + row];
						}
					}
					Result = Transform::ToTransform(m);
				}
				else
				{
					if (Node.has_translation)
					{
						Result.Position = glm::vec3(Node.translation[0], Node.translation[1], Node.translation[2]);
					}
					if (Node.has_rotation)
					{
						Result.Rotation = glm::quat(Node.rotation[3], Node.rotation[0], Node.rotation[1], Node.rotation[2]);
					}
					if (Node.has_scale)
					{
						Result.Scale = glm::vec3(Node.scale[0], Node.scale[1], Node.scale[2]);
					}
				}
				return Result;
			}

			auto GetNodeIndex(cgltf_node* Target, cgltf_node* AllNodes, u32 NumNodes) -> i32
			{
				if (!Target)
				{
					return -1;
				}

				for (u32 Index = 0; Index < NumNodes; Index++)
				{
					if (Target == &AllNodes[Index])
					{
						return (i32)Index;
					}
				}

				return -1;
			}

			auto LoadRestPose(cgltf_data* Data) -> Animation::FPose
			{
				Animation::FPose Result;

				u32 BoneCount = Data->nodes_count;
				Result.Joints.resize(BoneCount);
				Result.Parents.resize(BoneCount);
				for (u32 Index = 0; Index < BoneCount; Index++)
				{
					cgltf_node* Node = &(Data->nodes[Index]);
					Comp::FTransform Transform = GetLocalTransform(*Node);
					Result.Joints[Index] = Transform;
					i32 ParentIndex = GetNodeIndex(Node->parent, Data->nodes, BoneCount);
					Result.Parents[Index] = ParentIndex;
				}

				return Result;
			}

			auto LoadAnimationClips(cgltf_data* data) -> std::vector<Animation::FClip>
			{
				std::vector<Animation::FClip> Result;

				u32 numClips = data->animations_count;
				u32 numNodes = data->nodes_count;

				Result.resize(numClips);
				for (u32 i = 0; i < numClips; ++i)
				{
					auto& Clip = Result[i];
					Clip.Name = data->animations[i].name;
					u32 numChannels = data->animations[i].channels_count;
					for (u32 j = 0; j < numChannels; ++j)
					{
						cgltf_animation_channel& channel = data->animations[i].channels[j];
						cgltf_node* target = channel.target_node;
						int nodeId = GetNodeIndex(target, data->nodes, numNodes);
						if (channel.target_path == cgltf_animation_path_type_translation)
						{
							// Curve::FVecTrack& track = Clip[nodeId].Position;
							// TrackFromChannel<Curve::FVecTrack::TrackDataType, 3>(track, channel);
							// track.UpdateIndexLookupTable();
						}
						else if (channel.target_path == cgltf_animation_path_type_scale)
						{
							// Curve::FVecTrack& track = Clip[nodeId].Scale;
							// TrackFromChannel<Curve::FVecTrack::TrackDataType, 3>(track, channel);
							// track.UpdateIndexLookupTable();
						}
						else if (channel.target_path == cgltf_animation_path_type_rotation)
						{
							// Curve::FQuatTrack& track = Clip[nodeId].Rotation;
							// TrackFromChannel<Curve::FQuatTrack::TrackDataType, 4>(track, channel);
							// track.UpdateIndexLookupTable();
						}
					}

					// Anim::RecalculateDuration(Clip);
				}

				return Result;
			}
		} // namespace Utils

		struct FVertex
		{
			glm::vec3 Position;
			glm::vec3 Normal;
			glm::vec3 Tangent;
			glm::vec2 TexCoord;
			glm::vec4 Weights;
			glm::ivec4 Joints;
		};

		inline auto operator==(const FVertex& Lhs, const FVertex& Rhs) -> bool
		{
			return Lhs.Position == Rhs.Position && Lhs.Normal == Rhs.Normal && Lhs.Tangent == Rhs.Tangent && Lhs.TexCoord == Rhs.TexCoord
				&& Lhs.Weights == Rhs.Weights && Lhs.Joints == Rhs.Joints;
		}
		inline auto operator!=(const FVertex& Lhs, const FVertex& Rhs) -> bool
		{
			return !(Lhs == Rhs);
		}

		struct FSubmeshDataInternal
		{
			std::vector<u32> Indices;
			u32 IndexCount;
			EIndexType IndexType;
			EPrimitiveType PrimitiveType;
			u32 MaterialIndex;
		};

		struct FMeshDataInternal
		{
			std::vector<FVertex> Vertices;
			std::vector<FSubmeshDataInternal> SubmeshData;
			std::vector<std::string> Materials;
			std::string Name;
			std::string SkeletonName;
			bool bHasSkinData;
		};

		auto ToMeshData(FMeshDataInternal& Internal, FMeshData& MeshData) -> void
		{
			MeshData.Name = Internal.Name;
			MeshData.bHasSkeleton = Internal.bHasSkinData;
			MeshData.SkeletonName = Internal.SkeletonName;
			MeshData.Materials = Internal.Materials;

			FMeshDataStream& VertexStream = MeshData.VertexStream;
			VertexStream.Count = Internal.Vertices.size();
			VertexStream.Stride = sizeof(FVertex);
			if (!Internal.bHasSkinData)
			{
				VertexStream.Stride = offsetof(FVertex, TexCoord) + sizeof(FVertex::TexCoord);
			}

			VertexStream.Data.resize(VertexStream.Count * VertexStream.Stride);
			for (u32 Idx = 0; Idx < Internal.Vertices.size(); Idx++)
			{
				u8* DataPtr = (u8*)&VertexStream.Data[Idx * VertexStream.Stride];
				memcpy(DataPtr, &Internal.Vertices[Idx], VertexStream.Stride);
			}

			for (auto& ResSub : Internal.SubmeshData)
			{
				auto& Submesh = MeshData.Submeshes.emplace_back();
				Submesh.PrimitiveType = ResSub.PrimitiveType;
				Submesh.MaterialIndex = ResSub.MaterialIndex;

				auto& IndexStream = Submesh.IndexStream;
				IndexStream.Count = ResSub.IndexCount;
				IndexStream.Stride = ((usize)ResSub.IndexType + 1) * sizeof(u16);
				IndexStream.Data.resize(IndexStream.Count * IndexStream.Stride);
				for (u32 Idx = 0; Idx < ResSub.Indices.size(); Idx++)
				{
					u8* DataPtr = (u8*)&IndexStream.Data[Idx * IndexStream.Stride];
					memcpy(DataPtr, &ResSub.Indices[Idx], IndexStream.Stride);
				}
			}
		}

		auto LoadData(const char* Path) -> cgltf_data*
		{
			cgltf_data* Data = NULL;

			cgltf_options Options = {};
			cgltf_result result = cgltf_parse_file(&Options, Path, &Data);
			if (result != cgltf_result_success)
			{
				return nullptr;
			}
			result = cgltf_load_buffers(&Options, Data, Path);
			if (result != cgltf_result_success)
			{
				cgltf_free(Data);
				return nullptr;
			}
			result = cgltf_validate(Data);
			if (result != cgltf_result_success)
			{
				cgltf_free(Data);
				return nullptr;
			}

			return Data;
		}

		auto FreeData(cgltf_data* Data) -> void
		{
			if (Data)
			{
				cgltf_free(Data);
			}
		}

		auto MeshFromAttribute(std::vector<FVertex>& Vertices, cgltf_attribute& attribute, cgltf_skin* skin, cgltf_node* nodes, u32 nodeCount) -> void
		{
			cgltf_attribute_type attribType = attribute.type;
			cgltf_accessor& accessor = *attribute.data;
			u32 compCount = 0;
			if (accessor.type == cgltf_type_vec2)
			{
				compCount = 2;
			}
			else if (accessor.type == cgltf_type_vec3)
			{
				compCount = 3;
			}
			else if (accessor.type == cgltf_type_vec4)
			{
				compCount = 4;
			}

			std::vector<f32> values;
			Utils::GetScalarValues(values, compCount, accessor);
			u32 accessorCount = accessor.count;

			Vertices.resize(accessorCount);

			for (u32 Idx = 0; Idx < accessorCount; Idx++)
			{
				u32 Index = Idx * compCount;
				switch (attribType)
				{
					case cgltf_attribute_type_position:
					{
						Vertices[Idx].Position = glm::vec3(values[Index + 0], values[Index + 1], values[Index + 2]);
						break;
					}
					case cgltf_attribute_type_texcoord:
					{
						Vertices[Idx].TexCoord = glm::vec2(values[Index + 0], values[Index + 1]);
						break;
					}
					case cgltf_attribute_type_normal:
					{
						auto normal = glm::vec3(values[Index + 0], values[Index + 1], values[Index + 2]);
						if (glm::length(normal) < 0.000001f)
							normal = glm::vec3(0, 1, 0);
						Vertices[Idx].Normal = glm::normalize(normal);
						break;
					}
					case cgltf_attribute_type_tangent:
					{
						auto tangent = glm::vec3(values[Index + 0], values[Index + 1], values[Index + 2]);
						if (glm::length(tangent) < 0.000001f)
							tangent = glm::vec3(1, 0, 0);
						Vertices[Idx].Tangent = glm::normalize(tangent);
						break;
					}
					case cgltf_attribute_type_weights:
					{
						auto weight = glm::vec4(values[Index + 0], values[Index + 1], values[Index + 2], values[Index + 3]);
						f32 sum = weight.x + weight.y + weight.z + weight.w;
						if (sum > 0.0f)
							weight /= sum;
						Vertices[Idx].Weights = weight;
						break;
					}
					case cgltf_attribute_type_joints:
					{
						auto joints = glm::ivec4(
							(i32)(values[Index + 0] + 0.5f), (i32)(values[Index + 1] + 0.5f), (i32)(values[Index + 2] + 0.5f), (i32)(values[Index + 3] + 0.5f));
						for (u32 i = 0; i < 4; i++)
						{
							joints[i] = glm::clamp(joints[i], 0, (i32)skin->joints_count - 1);
							joints[i] = glm::max(0, Utils::GetNodeIndex(skin->joints[joints[i]], nodes, nodeCount));
						}
						Vertices[Idx].Joints = joints;
						break;
					}

					default:
						break;
				}
			}
		}

		auto LoadMesh(cgltf_data* data) -> std::vector<FMeshData>
		{
			std::vector<FMeshDataInternal> Result;

			cgltf_node* nodes = data->nodes;
			u32 nodeCount = data->nodes_count;

			for (u32 NodeIdx = 0; NodeIdx < nodeCount; NodeIdx++)
			{
				auto node = &nodes[NodeIdx];
				if (node->mesh == 0)
				{
					continue;
				}

				FMeshDataInternal& Mesh = Result.emplace_back();
				Mesh.Name = node->mesh->name;
				Mesh.bHasSkinData = node->skin != nullptr;
				if (Mesh.bHasSkinData)
				{
					Mesh.SkeletonName = node->skin->name;
				}

				u32 numPrims = node->mesh->primitives_count;
				for (u32 PrimIdx = 0; PrimIdx < numPrims; PrimIdx++)
				{
					auto primitive = &node->mesh->primitives[PrimIdx];
					u32 attribCount = primitive->attributes_count;
					std::vector<FVertex> PrimVertices;
					for (u32 AttribIdx = 0; AttribIdx < attribCount; AttribIdx++)
					{
						auto attribute = &primitive->attributes[AttribIdx];
						MeshFromAttribute(PrimVertices, *attribute, node->skin, nodes, nodeCount);
					}
					for (auto& Vert : PrimVertices)
					{
						Mesh.Vertices.emplace_back(Vert);
					}

					FSubmeshDataInternal& Submesh = Mesh.SubmeshData.emplace_back();
					Mesh.Materials.emplace_back(primitive->material->name ? primitive->material->name
																		  : ("material_" + std::to_string(cgltf_material_index(data, primitive->material))));
					if (primitive->indices)
					{
						Submesh.IndexCount = primitive->indices->count;
						Submesh.IndexType = EIndexType::UInt32;
						Submesh.PrimitiveType = EPrimitiveType::Triangles;
						auto& indices = Submesh.Indices;
						indices.resize(Submesh.IndexCount);
						for (u32 IndexIdx = 0; IndexIdx < Submesh.IndexCount; IndexIdx++)
						{
							indices[IndexIdx] = cgltf_accessor_read_index(primitive->indices, IndexIdx);
						}
					}
				}
			}

			std::vector<FMeshData> DataResult;
			DataResult.reserve(Result.size());
			for (auto& Res : Result)
			{
				FMeshData& MeshData = DataResult.emplace_back();
				ToMeshData(Res, MeshData);
			}

			return DataResult;
		}

		auto LoadSkeleton(cgltf_data* Data, cgltf_skin& Skin, Mesh::FSkeleton& Skeleton) -> void
		{
			u32 NodeCount = Data->nodes_count;

			Skeleton.Name = Skin.name;
			Skeleton.RestPose.Joints.resize(NodeCount);
			Skeleton.RestPose.Parents.resize(NodeCount, -1);
			Skeleton.JointNames.resize(NodeCount);
			Skeleton.InvBindPose.resize(NodeCount, glm::mat4(1.0f));

			for (u32 nodeIdx = 0; nodeIdx < NodeCount; ++nodeIdx)
			{
				const cgltf_node* node = &Data->nodes[nodeIdx];
				Skeleton.RestPose.Joints[nodeIdx] = Utils::GetLocalTransform(*node);
				if (node->parent)
				{
					Skeleton.RestPose.Parents[nodeIdx] = Utils::GetNodeIndex(node->parent, Data->nodes, NodeCount);
				}
				Skeleton.JointNames[nodeIdx] = node->name ? node->name : ("node_" + std::to_string(nodeIdx));
			}

			if (Skin.inverse_bind_matrices)
			{
				const cgltf_accessor* accessor = Skin.inverse_bind_matrices;
				if (accessor->type == cgltf_type_mat4 && accessor->component_type == cgltf_component_type_r_32f)
				{
					for (size_t i = 0; i < accessor->count && i < Skin.joints_count; ++i)
					{
						cgltf_float matrix[16];
						cgltf_accessor_read_float(accessor, i, matrix, 16);
						int nodeIdx = Utils::GetNodeIndex(Skin.joints[i], Data->nodes, NodeCount);
						if (nodeIdx >= 0 && (u32)nodeIdx < NodeCount)
						{
							glm::mat4 m;
							for (int col = 0; col < 4; ++col)
							{
								for (int row = 0; row < 4; ++row)
								{
									m[col][row] = matrix[col * 4 + row];
								}
							}
							Skeleton.InvBindPose[nodeIdx] = m;
						}
					}
				}
			}
		}

		auto LoadSkeleton(cgltf_data* Data) -> std::vector<Mesh::FSkeleton>
		{
			std::vector<Mesh::FSkeleton> Result;
			Result.reserve(Data->skins_count);
			if (Data->skins_count == 0)
			{
				return Result;
			}

			for (cgltf_size Idx = 0; Idx < Data->skins_count; Idx++)
			{
				if (Data->skins[Idx].joints_count == 0)
				{
					continue;
				}
				auto& Skeleton = Result.emplace_back();
				LoadSkeleton(Data, Data->skins[Idx], Skeleton);
			}

			return Result;
		}

		auto LoadImageData(const cgltf_image* image) -> FTexture;
		auto LoadMaterial(cgltf_data* data) -> std::vector<Mesh::FMaterial>
		{
			std::vector<FMaterial> Result;
			Result.reserve(data->materials_count);

			for (u32 i = 0; i < data->materials_count; i++)
			{
				const cgltf_material& srcMat = data->materials[i];
				FMaterial& Meterial = Result.emplace_back();

				const cgltf_pbr_metallic_roughness& pbr = srcMat.pbr_metallic_roughness;
				memcpy(&Meterial.BaseColorFactor.r, pbr.base_color_factor, sizeof(f32) * 4);
				Meterial.MetallicFactor = pbr.metallic_factor;
				Meterial.RoughnessFactor = pbr.roughness_factor;
				memcpy(&Meterial.EmissiveFactor.r, srcMat.emissive_factor, sizeof(f32) * 3);

				// Copy texture paths
				auto GetTexture = [](const cgltf_texture_view* view) -> FTexture {
					if (!view || !view->texture || !view->texture->image)
						return FTexture();
					return LoadImageData(view->texture->image);
				};
				Meterial.BaseColorTex = GetTexture(&pbr.base_color_texture);
				Meterial.BaseColorTex.Name =
					(pbr.base_color_texture.texture && pbr.base_color_texture.texture->name) ? pbr.base_color_texture.texture->name : "Diffuse";
				Meterial.MetallicRoughnessTex = GetTexture(&pbr.metallic_roughness_texture);
				Meterial.MetallicRoughnessTex.Name = (pbr.metallic_roughness_texture.texture && pbr.metallic_roughness_texture.texture->name)
					? pbr.metallic_roughness_texture.texture->name
					: "MetallicRoughness";
				Meterial.NormalTex = GetTexture(&srcMat.normal_texture);
				Meterial.NormalTex.Name =
					(srcMat.normal_texture.texture && srcMat.normal_texture.texture->name) ? srcMat.normal_texture.texture->name : "Normal";
				Meterial.EmissiveTex = GetTexture(&srcMat.emissive_texture);
				Meterial.EmissiveTex.Name =
					(srcMat.emissive_texture.texture && srcMat.emissive_texture.texture->name) ? srcMat.emissive_texture.texture->name : "Emissive";
				Meterial.OcclusionTex = GetTexture(&srcMat.occlusion_texture);
				Meterial.OcclusionTex.Name =
					(srcMat.occlusion_texture.texture && srcMat.occlusion_texture.texture->name) ? srcMat.occlusion_texture.texture->name : "Occlusion";

				// Other properties
				Meterial.DoubleSided = srcMat.double_sided;
				Meterial.Name = srcMat.name ? srcMat.name : ("material_" + std::to_string(i));
			}

			return Result;
		}

		auto LoadImage(void* image_data, usize image_size) -> FTexture
		{
			stbi_set_flip_vertically_on_load(true);

			int width, height, channels;
			unsigned char* data = stbi_load_from_memory((stbi_uc*)image_data, image_size, &width, &height, &channels, 4);
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

		auto LoadImageData(const cgltf_image* image) -> FTexture
		{
			void* data = nullptr;
			FTexture Tex;

			if (image->uri && !image->buffer_view)
			{
				Tex = LoadImage(image->uri);
			}
			else if (image->buffer_view)
			{
				// Case 2: Embedded in a buffer view
				cgltf_buffer_view* view = image->buffer_view;
				usize size = view->size;
				data = malloc(size);
				if (data)
				{
					memcpy(data, (char*)view->buffer->data + view->offset, size);
				}
				Tex = LoadImage(data, size);
				free(data);
			}
			else if (image->uri && image->mime_type)
			{
				char* base64_data = strchr(image->uri, ',');
				if (!base64_data)
					return FTexture();
				base64_data++; // Skip the comma

				usize size = cgltf_decode_string(base64_data);
				if (size == 0)
					return FTexture();

				cgltf_options options{};
				if (!(cgltf_load_buffer_base64(&options, size, base64_data, &data) != cgltf_result_success))
				{
					Tex = LoadImage(data, size);
					cgltf_default_free(nullptr, data);
				}
			}

			return Tex;
		}

		auto GetImageData(void* image_data, usize image_size) -> FTextureView
		{
			stbi_set_flip_vertically_on_load(true);

			int width, height, channels;
			unsigned char* data = stbi_load_from_memory((stbi_uc*)image_data, image_size, &width, &height, &channels, 4);

			// ASSERT(data);

			FTextureView Result;
			Result.Payload = data;
			Result.Width = width;
			Result.Height = height;
			Result.Channels = 4;
			Result.Size = Result.Width * Result.Height * Result.Channels;

			return Result;
		}

		auto GetImageData(const char* path) -> FTextureView
		{
			stbi_set_flip_vertically_on_load(true);

			int width, height, channels;
			unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
			channels = 4;

			// ASSERT(data);

			FTextureView Result;
			Result.Payload = data;
			Result.Width = width;
			Result.Height = height;
			Result.Channels = 4;
			Result.Size = Result.Width * Result.Height * Result.Channels;

			return Result;
		}

		auto GetTextureView(const cgltf_image& Image, const std::string& Alias) -> FTextureView
		{
			// Copy texture paths
			auto GetTextureName = [](const cgltf_image& image, const std::string& alias) -> std::string {
				if (image.name)
					return image.name;
				if (image.uri)
				{
					std::filesystem::path p(image.uri);
					return p.filename();
				}
				return alias;
			};

			void* data = nullptr;
			if (Image.uri && !Image.buffer_view)
			{
				auto Result = GetImageData(Image.uri);
				Result.Name = GetTextureName(Image, Alias);
				return Result;
			}
			else if (Image.buffer_view)
			{
				cgltf_buffer_view* view = Image.buffer_view;
				usize size = view->size;
				data = malloc(size);
				if (data)
				{
					memcpy(data, (char*)view->buffer->data + view->offset, size);
				}
				auto Result = GetImageData(data, size);
				Result.Name = GetTextureName(Image, Alias);
				free(data);
				return Result;
			}
			else if (Image.uri && Image.mime_type)
			{
				char* base64_data = strchr(Image.uri, ',');
				if (!base64_data)
					return FTextureView();
				base64_data++; // Skip the comma

				usize size = cgltf_decode_string(base64_data);
				if (size == 0)
					return FTextureView();

				FTextureView Result;
				cgltf_options options{};
				if (!(cgltf_load_buffer_base64(&options, size, base64_data, &data) != cgltf_result_success))
				{
					auto Result = GetImageData(data, size);
					Result.Name = GetTextureName(Image, Alias);
					cgltf_default_free(nullptr, data);
				}
				return Result;
			}

			return FTextureView();
		}

		using FTextureViewMap = std::unordered_map<const cgltf_image*, u32>;
		using FMaterialViewMap = std::unordered_map<const cgltf_material*, u32>;
		using FSkeletonViewMap = std::unordered_map<const cgltf_skin*, u32>;

		static auto LoadTextureView(cgltf_data* Data, FTextureViewMap& Map) -> std::vector<Mesh::FTextureView>
		{
			std::vector<Mesh::FTextureView> Result;
			Result.reserve(Data->images_count);

			for (u32 Idx = 0; Idx < Data->images_count; Idx++)
			{
				const cgltf_image& Image = Data->images[Idx];
				Map[&Image] = Idx;
				Result.emplace_back(GetTextureView(Image, std::format("Image_{}", Idx)));
			}

			return Result;
		}

		static auto GetMaterialView(const cgltf_material& SrcMat, const FTextureViewMap& TextureMap, const std::string Alias) -> Mesh::FMaterialView
		{
			auto RetriveTextureViewIndex = [](const FTextureViewMap& Map, const cgltf_texture_view* View) -> i32 {
				if (!View || !View->texture || !View->texture->image)
					return -1;
				if (!Map.contains(View->texture->image))
					return -1;
				return Map.at(View->texture->image);
			};

			Mesh::FMaterialView Result{};

			const cgltf_pbr_metallic_roughness& PBR = SrcMat.pbr_metallic_roughness;
			memcpy(&Result.BaseColorFactor.r, PBR.base_color_factor, sizeof(f32) * 4);
			Result.MetallicFactor = PBR.metallic_factor;
			Result.RoughnessFactor = PBR.roughness_factor;
			memcpy(&Result.EmissiveFactor.r, SrcMat.emissive_factor, sizeof(f32) * 3);

			Result.BaseColorTex = RetriveTextureViewIndex(TextureMap, &PBR.base_color_texture);
			Result.MetallicRoughnessTex = RetriveTextureViewIndex(TextureMap, &PBR.metallic_roughness_texture);
			Result.NormalTex = RetriveTextureViewIndex(TextureMap, &SrcMat.normal_texture);
			Result.EmissiveTex = RetriveTextureViewIndex(TextureMap, &SrcMat.emissive_texture);
			Result.OcclusionTex = RetriveTextureViewIndex(TextureMap, &SrcMat.occlusion_texture);

			// Other properties
			Result.DoubleSided = SrcMat.double_sided;
			Result.Name = SrcMat.name ? SrcMat.name : Alias;

			return Result;
		}

		static auto LoadMaterialView(cgltf_data* Data, FMaterialViewMap& Map, const FTextureViewMap& TextureMap) -> std::vector<Mesh::FMaterialView>
		{
			std::vector<Mesh::FMaterialView> Result;
			Result.reserve(Data->materials_count);

			for (u32 Idx = 0; Idx < Data->materials_count; Idx++)
			{
				const cgltf_material& SrcMat = Data->materials[Idx];
				Map[&SrcMat] = Idx;
				Result.emplace_back(GetMaterialView(SrcMat, TextureMap, std::format("Material_{}", Idx)));
			}

			return Result;
		}

		static auto LoadSkeletonView(cgltf_data* Data, FSkeletonViewMap& Map) -> std::vector<Mesh::FSkeletonView>
		{
			std::vector<Mesh::FSkeletonView> Result;
			Result.reserve(Data->skins_count);

			for (u32 Idx = 0; Idx < Data->skins_count; Idx++)
			{
				const cgltf_skin& Skin = Data->skins[Idx];
				Map[&Skin] = Idx;
				Result.emplace_back(Mesh::FSkeletonView{ .Name = Skin.name ? Skin.name : std::format("Skeleton_{}", Idx), .Payload = (void*)(&Skin) });
			}

			return Result;
		}

		static auto GetMeshView(const cgltf_node& Node, cgltf_data* Data, const FSkeletonViewMap& SkeletonMap, const FMaterialViewMap& MaterialMap,
			const std::string& Alias) -> Mesh::FMeshView
		{
			Mesh::FMeshView Result{};
			Result.MeshPayload = (void*)Node.mesh;
			Result.SkinPayload = (void*)Node.skin;
			Result.Name = Node.mesh->name ? Node.mesh->name : Alias;
			Result.SkeletonIndex = SkeletonMap.contains(Node.skin) ? SkeletonMap.at(Node.skin) : -1;

			u32 numPrims = Node.mesh->primitives_count;
			Result.Submeshes.reserve(numPrims);
			for (u32 PrimIdx = 0; PrimIdx < numPrims; PrimIdx++)
			{
				auto& primitive = Node.mesh->primitives[PrimIdx];
				// TODO: assert MaterialMap.contains(primitive->material)
				Result.Submeshes.emplace_back(Mesh::FMeshView::FSubmeshView{ .MaterialIndex = MaterialMap.at(primitive.material) });
			}

			return Result;
		}

		static auto LoadMeshView(cgltf_data* Data, FSkeletonViewMap& SkeletonMap, FMaterialViewMap& MaterialMap) -> std::vector<Mesh::FMeshView>
		{
			std::vector<Mesh::FMeshView> Result;
			Result.reserve(Data->meshes_count);

			u32 NodeCount = Data->nodes_count;
			for (u32 Idx = 0; Idx < NodeCount; Idx++)
			{
				auto& Node = Data->nodes[Idx];
				if (Node.mesh == 0)
				{
					continue;
				}

				Result.emplace_back(GetMeshView(Node, Data, SkeletonMap, MaterialMap, std::format("Mesh_{}", Idx)));
			}

			return Result;
		}

		auto LoadMeshView(cgltf_data* Data) -> FMeshLoadView
		{
			FMeshLoadView Result;

			Result.Payload = (void*)Data;
			Result.Type = FMeshLoadView::EPayloadType::GLTF;

			FTextureViewMap TexViewMap;
			FMaterialViewMap MatViewMap;
			FSkeletonViewMap SkViewMap;
			Result.Textures = LoadTextureView(Data, TexViewMap);
			Result.Materials = LoadMaterialView(Data, MatViewMap, TexViewMap);
			Result.Skeletons = LoadSkeletonView(Data, SkViewMap);
			Result.Meshes = LoadMeshView(Data, SkViewMap, MatViewMap);

			return Result;
		}

		auto ImportMesh(const FMeshLoadView& View, const std::filesystem::path& RootPath) -> void
		{
			namespace fs = std::filesystem;

			std::error_code ec;
			fs::create_directories(RootPath, ec);
			if (ec)
				std::cerr << "Failed: " << ec.message() << "\n";

			for (auto& Texture : View.Textures)
			{
				fs::path AssetPath = RootPath / Texture.Name;
				AssetPath.replace_extension(".png");

				Asset::FTexture TexAsset;
				TexAsset.Width = Texture.Width;
				TexAsset.Height = Texture.Height;
				TexAsset.Channels = Texture.Channels;
				TexAsset.Ref = Asset::MakeAssetRef(Asset::EAssetType::Texture, AssetPath);
				TexAsset.Pixels.Stride = Texture.Width * Texture.Channels;
				TexAsset.Pixels.Count = Texture.Height;
				TexAsset.Pixels.Data.resize(TexAsset.Pixels.Count * TexAsset.Pixels.Stride);
				memcpy(TexAsset.Pixels.Data.data(), Texture.Payload, Texture.Size);

				stbi_write_png(AssetPath.c_str(), TexAsset.Width, TexAsset.Height, TexAsset.Channels, TexAsset.Pixels.Data.data(), TexAsset.Pixels.Stride);
				AssetPath.replace_extension(".retex");
				std::ofstream OutStream(AssetPath);
				cereal::JSONOutputArchive OutArchive(OutStream);
				OutArchive(cereal::make_nvp(Texture.Name, TexAsset));
			}

			for (auto& Material : View.Materials)
			{
				Asset::FMaterial MatAsset;
				MatAsset.BaseColorFactor = Material.BaseColorFactor;
				MatAsset.MetallicFactor = Material.MetallicFactor;
				MatAsset.RoughnessFactor = Material.RoughnessFactor;
				MatAsset.EmissiveFactor = Material.EmissiveFactor;
				MatAsset.DoubleSided = Material.DoubleSided;
				if (Material.BaseColorTex >= 0 && Material.BaseColorTex < View.Textures.size())
				{
					auto& Texture = View.Textures[Material.BaseColorTex];
					auto TexAssetPath = (RootPath / Texture.Name).replace_extension(".retex");
					MatAsset.BaseColorTex = Asset::MakeAssetRef(Asset::EAssetType::Texture, TexAssetPath);
				}
				if (Material.MetallicRoughnessTex >= 0 && Material.MetallicRoughnessTex < View.Textures.size())
				{
					auto& Texture = View.Textures[Material.MetallicRoughnessTex];
					auto TexAssetPath = (RootPath / Texture.Name).replace_extension(".retex");
					MatAsset.MetallicRoughnessTex = Asset::MakeAssetRef(Asset::EAssetType::Texture, TexAssetPath);
				}
				if (Material.NormalTex >= 0 && Material.NormalTex < View.Textures.size())
				{
					auto& Texture = View.Textures[Material.NormalTex];
					auto TexAssetPath = (RootPath / Texture.Name).replace_extension(".retex");
					MatAsset.NormalTex = Asset::MakeAssetRef(Asset::EAssetType::Texture, TexAssetPath);
				}
				if (Material.EmissiveTex >= 0 && Material.EmissiveTex < View.Textures.size())
				{
					auto& Texture = View.Textures[Material.EmissiveTex];
					auto TexAssetPath = (RootPath / Texture.Name).replace_extension(".retex");
					MatAsset.EmissiveTex = Asset::MakeAssetRef(Asset::EAssetType::Texture, TexAssetPath);
				}
				if (Material.OcclusionTex >= 0 && Material.OcclusionTex < View.Textures.size())
				{
					auto& Texture = View.Textures[Material.OcclusionTex];
					auto TexAssetPath = (RootPath / Texture.Name).replace_extension(".retex");
					MatAsset.OcclusionTex = Asset::MakeAssetRef(Asset::EAssetType::Texture, TexAssetPath);
				}

				fs::path AssetPath = RootPath / Material.Name;
				AssetPath.replace_extension(".remat");
				std::ofstream OutStream(AssetPath);
				cereal::JSONOutputArchive OutArchive(OutStream);
				OutArchive(cereal::make_nvp(Material.Name, MatAsset));
			}

			for (auto& Sk : View.Skeletons)
			{
				Asset::FSkeleton SkAsset;

				// TODO: load skeleton

				fs::path AssetPath = RootPath / Sk.Name;
				AssetPath.replace_extension(".reskin");
				std::ofstream OutStream(AssetPath);
				cereal::JSONOutputArchive OutArchive(OutStream);
				OutArchive(cereal::make_nvp(Sk.Name, SkAsset));
			}

			for (auto& Mesh : View.Meshes)
			{
				if (Mesh.SkeletonIndex >= 0)
				{
					Asset::FSkinnedMesh MeshAsset;

					// TODO: load mesh data here

					auto& Sk = View.Skeletons[Mesh.SkeletonIndex];
					auto SkAssetPath = (RootPath / Sk.Name).replace_extension(".reskin");
					MeshAsset.Skeleton = Asset::MakeAssetRef(Asset::EAssetType::Skeleton, SkAssetPath);

					for (auto& Submesh : Mesh.Submeshes)
					{
						auto& Sub = MeshAsset.Submeshes.emplace_back();
						Sub.PrimitiveType = Mesh::EPrimitiveType::Triangles;

						// TODO: load submesh data here

						auto& Material = View.Materials[Submesh.MaterialIndex];
						auto MatAssetPath = (RootPath / Material.Name).replace_extension(".remat");
						Sub.Material = Asset::MakeAssetRef(Asset::EAssetType::Material, MatAssetPath);
					}

					fs::path AssetPath = RootPath / Mesh.Name;
					AssetPath.replace_extension(".reskmesh");
					std::ofstream OutStream(AssetPath);
					cereal::JSONOutputArchive OutArchive(OutStream);
					OutArchive(cereal::make_nvp(Mesh.Name, MeshAsset));
				}
				else
				{
					Asset::FStaticMesh MeshAsset;

					// TODO: load mesh data here

					for (auto& Submesh : Mesh.Submeshes)
					{
						auto& Sub = MeshAsset.Submeshes.emplace_back();
						Sub.PrimitiveType = Mesh::EPrimitiveType::Triangles;

						// TODO: load submesh data here

						auto& Material = View.Materials[Submesh.MaterialIndex];
						auto MatAssetPath = (RootPath / Material.Name).replace_extension(".remat");
						Sub.Material = Asset::MakeAssetRef(Asset::EAssetType::Material, MatAssetPath);
					}

					fs::path AssetPath = RootPath / Mesh.Name;
					AssetPath.replace_extension(".remesh");
					std::ofstream OutStream(AssetPath);
					cereal::JSONOutputArchive OutArchive(OutStream);
					OutArchive(cereal::make_nvp(Mesh.Name, MeshAsset));
				}
			}
		}
	} // namespace gltf
} // namespace Engine::Mesh
