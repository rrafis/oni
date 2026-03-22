#pragma once

#include <Platform.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_INTRINSICS
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace glm
{
	template <typename T, qualifier Q>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR glm::vec<3, T, Q> lerp(glm::vec<3, T, Q> const& x, glm::vec<3, T, Q> const& y, T a)
	{
		static_assert(std::numeric_limits<T>::is_iec559 || GLM_CONFIG_UNRESTRICTED_FLOAT, "'lerp' only accept floating-point inputs");

		// Lerp is only defined in [0, 1]
		assert(a >= static_cast<T>(0));
		assert(a <= static_cast<T>(1));

		return x * (static_cast<T>(1) - a) + (y * a);
	}
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR quat quat_identity()
	{
		return glm::quat_identity<f32, highp>();
	}

	GLM_FUNC_QUALIFIER dualquat dual_quat_identity()
	{
		return glm::dual_quat_identity<float, highp>();
	}

	GLM_FUNC_QUALIFIER GLM_CONSTEXPR float dot(dualquat const& p, dualquat const& o)
	{
		return glm::dot(p.real, o.real);
	}
} // namespace glm

namespace Engine
{

	struct FTransform
	{
		glm::vec3 Position = glm::vec3();
		glm::quat Rotation = glm::quat_identity();
		glm::vec3 Scale = glm::vec3(1.0f);
	};

	namespace Animation
	{
		struct FPose
		{
			std::vector<FTransform> Joints;
			std::vector<i32> Parents;
		};

		struct FTransformTrack
		{
			u32 BoneId;
			// Curve::FVecTrack Position;
			// Curve::FQuatTrack Rotation;
			// Curve::FVecTrack Scale;

			inline auto TimeStart() -> f32
			{
				// return glm::min(glm::min(Position.TimeStart(), Rotation.TimeStart()), Scale.TimeStart());
				return 0.0f;
			}

			inline auto TimeEnd() -> f32
			{
				// return glm::max(glm::max(Position.TimeEnd(), Rotation.TimeEnd()), Scale.TimeEnd());
				return 0.0f;
			}

			auto Sample(const FTransform& Base, f32 AtTime, bool bLooping) -> FTransform
			{
				FTransform Result = Base;

				// if (Position.Frames.size() > 1)
				// {
				// 	Result.Position = Position.Sample(AtTime, bLooping);
				// }

				// if (Rotation.Frames.size() > 1)
				// {
				// 	Result.Rotation = Rotation.Sample(AtTime, bLooping);
				// }

				// if (Scale.Frames.size() > 1)
				// {
				// 	Result.Scale = Scale.Sample(AtTime, bLooping);
				// }

				return Result;
			}
		};

		struct FClip
		{
			std::vector<FTransformTrack> Tracks;
			f32 StartTime = 0.0f, EndTime = 0.0f;
			bool bLooping = true;
			std::string Name = "No name";

			auto operator[](u32 BoneId) -> FTransformTrack&;

			auto Sample(FPose& OutPose, f32 AtTime) -> f32;

			inline constexpr auto GetDuration() -> f32
			{
				return EndTime - StartTime;
			}

		private:
			auto AdjustTimeToFitTrack(f32 Time) -> f32;
		};
	} // namespace Animation

	namespace Mesh
	{
		enum class EIndexType : u8
		{
			UInt16,
			UInt32
		};

		enum class EPrimitiveType : u8
		{
			Triangles,
			Lines
		};

		struct FMeshDataStream
		{
			u64 Count;
			u32 Stride;
			std::vector<u8> Data; // raw index data
		};

		struct FMaterial
		{
			glm::vec4 BaseColorFactor = { 1, 1, 1, 1 };
			f32 MetallicFactor = 1.0f;
			f32 RoughnessFactor = 1.0f;
			glm::vec4 EmissiveFactor = { 0, 0, 0, 0 };
			std::string BaseColorTex;
			std::string MetallicRoughnessTex;
			std::string NormalTex;
			std::string EmissiveTex;
			std::string OcclusionTex;
			bool DoubleSided = false;
			std::string Name;
		};

		struct FSubmeshData
		{
			FMeshDataStream IndexStream;
			EPrimitiveType PrimitiveType;
			u32 MaterialIndex;

			// Future-proof extras
			f32 BoundingSphere[4]; // xyz center + radius
								   // Or AABB: min[3], max[3]
		};

		struct FMeshData
		{
			FMeshDataStream VertexStream;
			std::vector<FSubmeshData> Submeshes;
			std::vector<std::string> Materials;
			std::string Name;
			std::string SkeletonName;
			bool bHasSkeleton;
		};

		struct FSkeleton
		{
			Animation::FPose RestPose;
			std::vector<glm::mat4> InvBindPose;
			std::vector<std::string> JointNames;
			std::string Name;
		};
	} // namespace Mesh

	namespace Transform
	{
		auto Combine(const FTransform& Lhs, const FTransform& Rhs) -> FTransform;
		auto Inverse(const FTransform& In) -> FTransform;
		auto Mix(const FTransform& Lhs, const FTransform& Rhs, f32 Time) -> FTransform;

		auto ToMatrix(const FTransform& In) -> glm::mat4;

		auto ToTransform(const glm::mat4& Matrix) -> FTransform;
		auto ToTransform(const glm::dualquat& DualQuat) -> FTransform;

		auto ToDualQuat(const FTransform& Transform) -> glm::dualquat;
		auto ToDualQuat(const glm::mat4& Matrix) -> glm::dualquat;

		auto Point(const FTransform& In, const glm::vec3& Point) -> glm::vec3;
		auto Point(const glm::mat4& M, const glm::vec3& p) -> glm::vec3;
		auto Point(const glm::dualquat& dq, const glm::vec3& p) -> glm::vec3;

		auto Vector(const FTransform& In, const glm::vec3& Vector) -> glm::vec3;
		auto Vector(const glm::mat4& M, const glm::vec3& v) -> glm::vec3;
		auto Vector(const glm::dualquat& dq, const glm::vec3& v) -> glm::vec3;
	} // namespace Transform
} // namespace Engine
