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
			ArgTableVertDesc->setMaxBufferBindCount(5);
			ArgTableVertDesc->setMaxTextureBindCount(0);
			ArgTableVertDesc->setMaxSamplerStateBindCount(0);
			MeshRender.ArgTableVert = NS::TransferPtr(Device->newArgumentTable(ArgTableVertDesc.get(), &error));

			MeshRender.ArgTableVert->setAddress(MeshRender.VertexBuffer->gpuAddress(), 0);
			MeshRender.ArgTableVert->setAddress(MeshRender.UniformBuffer->gpuAddress(), 1);
			MeshRender.ResidencySet->addAllocation(MeshRender.VertexBuffer.get());
			MeshRender.ResidencySet->addAllocation(MeshRender.UniformBuffer.get());
			MeshRender.ResidencySet->addAllocation(MeshRender.MaterialIndexBuffer.get());

			NS::SharedPtr<MTL4::ArgumentTableDescriptor> ArgTableFragDesc = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
			ArgTableFragDesc->setMaxBufferBindCount(2);
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
				Sub.ArgTableFrag->setAddress(MeshRender.MaterialIndexBuffer->gpuAddress() + Idx * sizeof(u32), 1);
				MeshRender.ResidencySet->addAllocation(Sub.IndexBuffer.get());

				MeshRender.Submeshes.emplace_back(Sub);

				Idx++;
			}
			MeshRender.ResidencySet->commit();

			return MeshRender;
		}

		auto ProcessSkeletalMeshData(MTL::Device* Device, const MDLIO::FSkMeshData& MeshData, u32 MaterialIndexOffset) -> FSkMeshRenderer
		{
			FSkMeshRenderer Result;
			Result.BindTransoformBuffer = NS::TransferPtr(
				Device->newBuffer(MeshData.BindTransforms.data(), sizeof(glm::mat4) * MeshData.BindTransforms.size(), MTL::ResourceStorageModeShared));
			Result.PoseTransoformBuffer = NS::TransferPtr(
				Device->newBuffer(MeshData.RestTransforms.data(), sizeof(glm::mat4) * MeshData.RestTransforms.size(), MTL::ResourceStorageModeShared));
			for (auto& Mesh : MeshData.Meshes)
			{
				FSkeletalMeshRenderer MeshRender(ProcessMeshData(Device, Mesh, MaterialIndexOffset));
				MaterialIndexOffset += Mesh.Materials.size();
				MeshRender.SkinBuffer =
					NS::TransferPtr(Device->newBuffer(Mesh.SkinVertexStream.Data.data(), Mesh.SkinVertexStream.Data.size(), MTL::ResourceStorageModeShared));
				MeshRender.ArgTableVert->setAddress(MeshRender.SkinBuffer->gpuAddress(), 2);
				MeshRender.ArgTableVert->setAddress(Result.BindTransoformBuffer->gpuAddress(), 3);
				MeshRender.ArgTableVert->setAddress(Result.PoseTransoformBuffer->gpuAddress(), 4);
				MeshRender.ResidencySet->addAllocation(MeshRender.SkinBuffer.get());
				MeshRender.ResidencySet->addAllocation(Result.BindTransoformBuffer.get());
				MeshRender.ResidencySet->addAllocation(Result.PoseTransoformBuffer.get());
				MeshRender.ResidencySet->commit();
				Result.MeshRender.emplace_back(MeshRender);
			}
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

			auto GetLocalTransform(const cgltf_node& Node) -> FTransform
			{
				FTransform Result;
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
					FTransform Transform = GetLocalTransform(*Node);
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
			glm::vec2 TexCoord;
			glm::vec4 Weights;
			glm::ivec4 Joints;
		};

		inline auto operator==(const FVertex& Lhs, const FVertex& Rhs) -> bool
		{
			return Lhs.Position == Rhs.Position && Lhs.Normal == Rhs.Normal && Lhs.TexCoord == Rhs.TexCoord && Lhs.Weights == Rhs.Weights
				&& Lhs.Joints == Rhs.Joints;
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
						auto nornal = glm::vec3(values[Index + 0], values[Index + 1], values[Index + 2]);
						if (glm::length(nornal) < 0.000001f)
							nornal = glm::vec3(0, 1, 0);
						Vertices[Idx].Normal = glm::normalize(nornal);
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

		auto LoadMaterial(cgltf_data* data) -> std::vector<Mesh::FMaterial>
		{
			std::vector<FMaterial> Result;
			Result.reserve(data->materials_count);

			for (u32 i = 0; i < data->materials_count; i++)
			{
				const cgltf_material& srcMat = data->materials[i];
				FMaterial& Meterial = Result.emplace_back();

				const cgltf_pbr_metallic_roughness& pbr = srcMat.pbr_metallic_roughness;
				memcpy(&Meterial.BaseColorFactor.r, pbr.base_color_factor, sizeof(float) * 4);
				Meterial.MetallicFactor = pbr.metallic_factor;
				Meterial.RoughnessFactor = pbr.roughness_factor;
				memcpy(&Meterial.EmissiveFactor.r, srcMat.emissive_factor, sizeof(float) * 3);

				// Copy texture paths
				auto GetTexturePath = [](const cgltf_texture_view* view) -> std::string {
					if (!view || !view->texture || !view->texture->image || !view->texture->image->uri)
						return "";
					return view->texture->image->uri;
				};
				Meterial.BaseColorTex = GetTexturePath(&pbr.base_color_texture);
				Meterial.MetallicRoughnessTex = GetTexturePath(&pbr.metallic_roughness_texture);
				Meterial.NormalTex = GetTexturePath(&srcMat.normal_texture);
				Meterial.EmissiveTex = GetTexturePath(&srcMat.emissive_texture);
				Meterial.OcclusionTex = GetTexturePath(&srcMat.occlusion_texture);

				// Other properties
				Meterial.DoubleSided = srcMat.double_sided;
				Meterial.Name = srcMat.name ? srcMat.name : ("material_" + std::to_string(i));
			}

			return Result;
		}
	} // namespace gltf

	auto GenerateSphere(const std::string& Name, f32 Radius, u32 Segments) -> FMeshData
	{
		gltf::FMeshDataInternal Internal{};
		Internal.bHasSkinData = false;
		Internal.Name = Name;
		Internal.SubmeshData.resize(1);
		Internal.SubmeshData[0].IndexType = EIndexType::UInt32;
		Internal.SubmeshData[0].MaterialIndex = 0;
		Internal.SubmeshData[0].PrimitiveType = EPrimitiveType::Triangles;
		Internal.Materials.emplace_back("Default");

		std::vector<gltf::FVertex>& vertices = Internal.Vertices;
		std::vector<u32>& indices = Internal.SubmeshData[0].Indices;

		const u32 stacks = Segments;
		const u32 slices = Segments;

		for (u32 t = 0; t <= stacks; t++)
		{
			f32 theta = (f32)t / stacks * glm::pi<f32>();
			f32 y = cos(theta);
			f32 r = sin(theta);

			for (u32 p = 0; p <= slices; p++)
			{
				f32 phi = (f32)p / slices * 2.0f * glm::pi<f32>();
				f32 x = r * cos(phi);
				f32 z = r * sin(phi);

				gltf::FVertex v;
				v.Position = glm::vec3(x, y, z) * Radius;
				v.Normal = glm::normalize(v.Position);
				v.TexCoord = glm::vec2((f32)p / slices, (f32)t / stacks);
				vertices.push_back(v);
			}
		}

		for (u32 t = 0; t < stacks; t++)
		{
			for (u32 p = 0; p < slices; p++)
			{
				u32 v0 = t * (slices + 1) + p;
				u32 v1 = v0 + 1;
				u32 v2 = (t + 1) * (slices + 1) + p;
				u32 v3 = v2 + 1;

				indices.push_back(v0);
				indices.push_back(v2);
				indices.push_back(v1);
				indices.push_back(v1);
				indices.push_back(v2);
				indices.push_back(v3);
			}
		}
		Internal.SubmeshData[0].IndexCount = (u32)indices.size();

		FMeshData Result;
		gltf::ToMeshData(Internal, Result);
		return Result;
	}

	auto GenerateBox(const std::string& Name, glm::vec3 Extent, glm::uvec3 Segments) -> FMeshData
	{
		gltf::FMeshDataInternal Internal{};
		Internal.bHasSkinData = false;
		Internal.Name = Name;
		Internal.SubmeshData.resize(1);
		Internal.SubmeshData[0].IndexType = EIndexType::UInt32;
		Internal.SubmeshData[0].MaterialIndex = 0;
		Internal.SubmeshData[0].PrimitiveType = EPrimitiveType::Triangles;
		Internal.Materials.emplace_back("Default");

		std::vector<gltf::FVertex>& vertices = Internal.Vertices;
		std::vector<u32>& indices = Internal.SubmeshData[0].Indices;

		auto AddFace = [&](glm::vec3 normal, glm::vec3 tangent, glm::vec3 bitangent, glm::vec2 uvScale) {
			for (u32 y = 0; y <= Segments.y; y++)
			{
				for (u32 x = 0; x <= Segments.x; x++)
				{
					f32 u = (f32)x / Segments.x;
					f32 v = (f32)y / Segments.y;
					glm::vec3 pos = normal * Extent + tangent * (u * 2.0f - 1.0f) * Extent.x + bitangent * (v * 2.0f - 1.0f) * Extent.y;
					gltf::FVertex vtx;
					vtx.Position = pos;
					vtx.Normal = normal;
					vtx.TexCoord = glm::vec2(u, v) * uvScale;
					vertices.push_back(vtx);
				}
			}
		};

		// Generate 6 faces (order: +X, -X, +Y, -Y, +Z, -Z)
		AddFace({ 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, { 1, 1 });
		AddFace({ -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 1, 1 });
		AddFace({ 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1 });
		AddFace({ 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 1, 1 });
		AddFace({ 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1 });
		AddFace({ 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 }, { 1, 1 });

		// Create indices for each face
		for (u32 face = 0; face < 6; face++)
		{
			u32 base = face * (Segments.x + 1) * (Segments.y + 1);
			for (u32 y = 0; y < Segments.y; y++)
			{
				for (u32 x = 0; x < Segments.x; x++)
				{
					u32 v0 = base + y * (Segments.x + 1) + x;
					u32 v1 = v0 + 1;
					u32 v2 = v0 + (Segments.x + 1);
					u32 v3 = v2 + 1;

					indices.push_back(v0);
					indices.push_back(v2);
					indices.push_back(v1);
					indices.push_back(v1);
					indices.push_back(v2);
					indices.push_back(v3);
				}
			}
		}
		Internal.SubmeshData[0].IndexCount = (u32)indices.size();

		FMeshData Result;
		gltf::ToMeshData(Internal, Result);
		return Result;
	}

	auto GenerateCapsule(const std::string& Name, glm::vec3 Extent, glm::uvec3 Segments) -> FMeshData
	{
		gltf::FMeshDataInternal Internal{};
		Internal.bHasSkinData = false;
		Internal.Name = Name;
		Internal.SubmeshData.resize(1);
		Internal.SubmeshData[0].IndexType = EIndexType::UInt32;
		Internal.SubmeshData[0].MaterialIndex = 0;
		Internal.SubmeshData[0].PrimitiveType = EPrimitiveType::Triangles;
		Internal.Materials.emplace_back("Default");

		std::vector<gltf::FVertex>& vertices = Internal.Vertices;
		std::vector<u32>& indices = Internal.SubmeshData[0].Indices;

		f32 radius = glm::min(Extent.x, Extent.z);
		f32 halfHeight = Extent.y - radius;

		// Generate cylinder body
		for (u32 t = 0; t <= Segments.y; t++)
		{
			f32 y = (f32)t / Segments.y * 2.0f * halfHeight - halfHeight;
			for (u32 p = 0; p <= Segments.x; p++)
			{
				f32 phi = (f32)p / Segments.x * 2.0f * glm::pi<f32>();
				f32 x = cos(phi) * radius;
				f32 z = sin(phi) * radius;

				gltf::FVertex v;
				v.Position = glm::vec3(x, y, z);
				v.Normal = glm::normalize(glm::vec3(x, 0, z));
				v.TexCoord = glm::vec2((f32)p / Segments.x, (f32)t / Segments.y);
				vertices.push_back(v);
			}
		}

		// Generate top hemisphere
		for (u32 t = 0; t <= Segments.y / 2; t++)
		{
			f32 theta = (f32)t / (Segments.y / 2) * glm::pi<f32>() / 2.0f;
			f32 y = sin(theta) * radius + halfHeight;
			f32 r = cos(theta) * radius;
			for (u32 p = 0; p <= Segments.x; p++)
			{
				f32 phi = (f32)p / Segments.x * 2.0f * glm::pi<f32>();
				f32 x = cos(phi) * r;
				f32 z = sin(phi) * r;

				gltf::FVertex v;
				v.Position = glm::vec3(x, y, z);
				v.Normal = glm::normalize(v.Position - glm::vec3(0, halfHeight, 0));
				v.TexCoord = glm::vec2((f32)p / Segments.x, (f32)t / (Segments.y / 2));
				vertices.push_back(v);
			}
		}

		// Generate bottom hemisphere
		for (u32 t = 0; t <= Segments.y / 2; t++)
		{
			f32 theta = (f32)t / (Segments.y / 2) * glm::pi<f32>() / 2.0f;
			f32 y = -sin(theta) * radius - halfHeight;
			f32 r = cos(theta) * radius;
			for (u32 p = 0; p <= Segments.x; p++)
			{
				f32 phi = (f32)p / Segments.x * 2.0f * glm::pi<f32>();
				f32 x = cos(phi) * r;
				f32 z = sin(phi) * r;

				gltf::FVertex v;
				v.Position = glm::vec3(x, y, z);
				v.Normal = glm::normalize(v.Position - glm::vec3(0, -halfHeight, 0));
				v.TexCoord = glm::vec2((f32)p / Segments.x, (f32)t / (Segments.y / 2));
				vertices.push_back(v);
			}
		}

		// Create indices for cylinder body
		u32 base = 0;
		for (u32 t = 0; t < Segments.y; t++)
		{
			for (u32 p = 0; p < Segments.x; p++)
			{
				u32 v0 = base + t * (Segments.x + 1) + p;
				u32 v1 = v0 + 1;
				u32 v2 = v0 + (Segments.x + 1);
				u32 v3 = v2 + 1;

				indices.push_back(v0);
				indices.push_back(v2);
				indices.push_back(v1);
				indices.push_back(v1);
				indices.push_back(v2);
				indices.push_back(v3);
			}
		}

		// Create indices for top hemisphere
		base = (Segments.y + 1) * (Segments.x + 1);
		for (u32 t = 0; t < Segments.y / 2; t++)
		{
			for (u32 p = 0; p < Segments.x; p++)
			{
				u32 v0 = base + t * (Segments.x + 1) + p;
				u32 v1 = v0 + 1;
				u32 v2 = v0 + (Segments.x + 1);
				u32 v3 = v2 + 1;

				indices.push_back(v0);
				indices.push_back(v2);
				indices.push_back(v1);
				indices.push_back(v1);
				indices.push_back(v2);
				indices.push_back(v3);
			}
		}

		// Create indices for bottom hemisphere
		base = (Segments.y + 1) * (Segments.x + 1) + (Segments.y / 2 + 1) * (Segments.x + 1);
		for (u32 t = 0; t < Segments.y / 2; t++)
		{
			for (u32 p = 0; p < Segments.x; p++)
			{
				u32 v0 = base + t * (Segments.x + 1) + p;
				u32 v1 = v0 + 1;
				u32 v2 = v0 + (Segments.x + 1);
				u32 v3 = v2 + 1;

				indices.push_back(v0);
				indices.push_back(v2);
				indices.push_back(v1);
				indices.push_back(v1);
				indices.push_back(v2);
				indices.push_back(v3);
			}
		}
		Internal.SubmeshData[0].IndexCount = (u32)indices.size();

		FMeshData Result;
		gltf::ToMeshData(Internal, Result);
		return Result;
	}
} // namespace Engine::Mesh
