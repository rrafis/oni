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

namespace Engine::Mesh
{
	void FMeshDataStream::Serialize(cereal::JSONOutputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Count) & CEREAL_NVP(Stride); // & Data;
		}
	}

	void FMeshDataStream::Deserialize(cereal::JSONInputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Count) & CEREAL_NVP(Stride); // & Data;
		}
	}

	void FMaterial::Serialize(cereal::JSONOutputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(BaseColorFactor) & CEREAL_NVP(MetallicFactor) & CEREAL_NVP(RoughnessFactor) & CEREAL_NVP(EmissiveFactor) & CEREAL_NVP(BaseColorTex)
				& CEREAL_NVP(MetallicRoughnessTex) & CEREAL_NVP(NormalTex) & CEREAL_NVP(EmissiveTex) & CEREAL_NVP(OcclusionTex) & CEREAL_NVP(DoubleSided)
				& CEREAL_NVP(Name);
		}
	}

	void FMaterial::Deserialize(cereal::JSONInputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(BaseColorFactor) & CEREAL_NVP(MetallicFactor) & CEREAL_NVP(RoughnessFactor) & CEREAL_NVP(EmissiveFactor) & CEREAL_NVP(BaseColorTex)
				& CEREAL_NVP(MetallicRoughnessTex) & CEREAL_NVP(NormalTex) & CEREAL_NVP(EmissiveTex) & CEREAL_NVP(OcclusionTex) & CEREAL_NVP(DoubleSided)
				& CEREAL_NVP(Name);
		}
	}

	void FSubmeshData::Serialize(cereal::JSONOutputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(PrimitiveType) & CEREAL_NVP(MaterialIndex) & CEREAL_NVP(IndexStream);
		}
	}
	void FSubmeshData::Deserialize(cereal::JSONInputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(PrimitiveType) & CEREAL_NVP(MaterialIndex) & CEREAL_NVP(IndexStream);
		}
	}

	void FMeshData::Serialize(cereal::JSONOutputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Name) & CEREAL_NVP(bHasSkeleton) & CEREAL_NVP(SkeletonName) & CEREAL_NVP(Materials) & CEREAL_NVP(VertexStream)
				& CEREAL_NVP(Submeshes);
		}
	}

	void FMeshData::Deserialize(cereal::JSONInputArchive& ar, const uint32_t version)
	{
		if (version <= 1)
		{
			ar& CEREAL_NVP(Name) & CEREAL_NVP(bHasSkeleton) & CEREAL_NVP(SkeletonName) & CEREAL_NVP(Materials) & CEREAL_NVP(VertexStream)
				& CEREAL_NVP(Submeshes);
		}
	}
} // namespace Engine::Mesh
