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
