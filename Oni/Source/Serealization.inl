#pragma once

#include "Types.h"

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

CEREAL_CLASS_VERSION(Engine::Mesh::FMeshDataStream, 1)
CEREAL_CLASS_VERSION(Engine::Mesh::FMaterial, 1)
CEREAL_CLASS_VERSION(Engine::Mesh::FSubmeshData, 1)
CEREAL_CLASS_VERSION(Engine::Mesh::FMeshData, 1)

CEREAL_CLASS_VERSION(Engine::Asset::FAssetRef, 1)
CEREAL_CLASS_VERSION(Engine::Asset::FTexture, 1)
CEREAL_CLASS_VERSION(Engine::Asset::FMaterial, 1)
CEREAL_CLASS_VERSION(Engine::Asset::FSkeleton, 1)
CEREAL_CLASS_VERSION(Engine::Asset::FStaticMesh, 1)
CEREAL_CLASS_VERSION(Engine::Asset::FSkinnedMesh, 1)

#include <cereal/cereal.hpp>
#include <glm/glm.hpp>

namespace glm
{
	template <class Archive>
	void serialize(Archive& ar, glm::vec4& v)
	{
		ar(v.x, v.y, v.z, v.w);
	}
} // namespace glm

namespace Engine::Mesh
{
	void FMeshDataStream::serialize(cereal::JSONInputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Count) & CEREAL_NVP(Stride); // & Data;
		}
	}

	void FMeshDataStream::serialize(cereal::JSONOutputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Count) & CEREAL_NVP(Stride); // & Data;
		}
	}

	void FMaterial::serialize(cereal::JSONInputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(BaseColorFactor) & CEREAL_NVP(MetallicFactor) & CEREAL_NVP(RoughnessFactor) & CEREAL_NVP(EmissiveFactor) & CEREAL_NVP(DoubleSided)
				& CEREAL_NVP(Name);
			// & CEREAL_NVP(BaseColorTex) & CEREAL_NVP(MetallicRoughnessTex) & CEREAL_NVP(NormalTex) & CEREAL_NVP(EmissiveTex) & CEREAL_NVP(OcclusionTex)
		}
	}

	void FMaterial::serialize(cereal::JSONOutputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(BaseColorFactor) & CEREAL_NVP(MetallicFactor) & CEREAL_NVP(RoughnessFactor) & CEREAL_NVP(EmissiveFactor) & CEREAL_NVP(DoubleSided)
				& CEREAL_NVP(Name);
			// & CEREAL_NVP(BaseColorTex) & CEREAL_NVP(MetallicRoughnessTex) & CEREAL_NVP(NormalTex) & CEREAL_NVP(EmissiveTex) & CEREAL_NVP(OcclusionTex)
		}
	}

	void FSubmeshData::serialize(cereal::JSONInputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(PrimitiveType) & CEREAL_NVP(MaterialIndex) & CEREAL_NVP(IndexStream);
		}
	}

	void FSubmeshData::serialize(cereal::JSONOutputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(PrimitiveType) & CEREAL_NVP(MaterialIndex) & CEREAL_NVP(IndexStream);
		}
	}

	void FMeshData::serialize(cereal::JSONInputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Name) & CEREAL_NVP(bHasSkeleton) & CEREAL_NVP(SkeletonName) & CEREAL_NVP(Materials) & CEREAL_NVP(VertexStream)
				& CEREAL_NVP(Submeshes);
		}
	}

	void FMeshData::serialize(cereal::JSONOutputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Name) & CEREAL_NVP(bHasSkeleton) & CEREAL_NVP(SkeletonName) & CEREAL_NVP(Materials) & CEREAL_NVP(VertexStream)
				& CEREAL_NVP(Submeshes);
		}
	}
} // namespace Engine::Mesh

namespace Engine::Asset
{

	auto FAssetRef::serialize(cereal::JSONInputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Type) & CEREAL_NVP(Path);
		}
	}
	auto FAssetRef::serialize(cereal::JSONOutputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Type) & CEREAL_NVP(Path);
		}
	}

	auto FDataStream::serialize(cereal::JSONInputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Count) & CEREAL_NVP(Stride); // & Data;
		}
	}
	auto FDataStream::serialize(cereal::JSONOutputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Count) & CEREAL_NVP(Stride); // & Data;
		}
	}

	auto FTexture::serialize(cereal::JSONInputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Width) & CEREAL_NVP(Height) & CEREAL_NVP(Channels) & CEREAL_NVP(Ref); // & Data;
		}
	}
	auto FTexture::serialize(cereal::JSONOutputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Width) & CEREAL_NVP(Height) & CEREAL_NVP(Channels) & CEREAL_NVP(Ref); // & Data;
		}
	}

	auto FMaterial::serialize(cereal::JSONInputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(BaseColorFactor) & CEREAL_NVP(MetallicFactor) & CEREAL_NVP(RoughnessFactor) & CEREAL_NVP(EmissiveFactor) & CEREAL_NVP(DoubleSided)
				& CEREAL_NVP(BaseColorTex) & CEREAL_NVP(MetallicRoughnessTex) & CEREAL_NVP(NormalTex) & CEREAL_NVP(EmissiveTex) & CEREAL_NVP(OcclusionTex);
		}
	}
	auto FMaterial::serialize(cereal::JSONOutputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(BaseColorFactor) & CEREAL_NVP(MetallicFactor) & CEREAL_NVP(RoughnessFactor) & CEREAL_NVP(EmissiveFactor) & CEREAL_NVP(DoubleSided)
				& CEREAL_NVP(BaseColorTex) & CEREAL_NVP(MetallicRoughnessTex) & CEREAL_NVP(NormalTex) & CEREAL_NVP(EmissiveTex) & CEREAL_NVP(OcclusionTex);
		}
	}

	auto FSkeleton::serialize(cereal::JSONInputArchive& ar, const uint32_t version) -> void {}
	auto FSkeleton::serialize(cereal::JSONOutputArchive& ar, const uint32_t version) -> void {}

	auto FStaticMesh::FSubmesh::serialize(cereal::JSONInputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(PrimitiveType) & CEREAL_NVP(Material); // & CEREAL_NVP(IndexStream);
		}
	}
	auto FStaticMesh::FSubmesh::serialize(cereal::JSONOutputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(PrimitiveType) & CEREAL_NVP(Material); // & CEREAL_NVP(IndexStream);
		}
	}

	auto FStaticMesh::serialize(cereal::JSONInputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Submeshes); // & CEREAL_NVP(VertexStream);
		}
	}
	auto FStaticMesh::serialize(cereal::JSONOutputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Submeshes); // & CEREAL_NVP(VertexStream);
		}
	}

	auto FSkinnedMesh::serialize(cereal::JSONInputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Skeleton) & CEREAL_NVP(Submeshes); // & CEREAL_NVP(VertexStream);
		}
	}
	auto FSkinnedMesh::serialize(cereal::JSONOutputArchive& ar, const uint32_t version) -> void
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Skeleton) & CEREAL_NVP(Submeshes); // & CEREAL_NVP(VertexStream);
		}
	}

} // namespace Engine::Asset
