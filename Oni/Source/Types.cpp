#include "Types.h"

namespace Engine::Animation
{
	auto FPose::GetGlobaMatrix(u32 Joint) const -> glm::mat4
	{
		glm::mat4 Result = Transform::ToMatrix(Joints[Joint]);
		for (int p = Parents[Joint]; p >= 0; p = Parents[p])
		{
			auto Parent = Transform::ToMatrix(Joints[p]);
			Result = Parent * Result; // Apply parent transform
		}
		return Result;
	}

	auto FPose::GetMatrixPalette() const -> std::vector<glm::mat4>
	{
		std::vector<glm::mat4> Result;
		Result.reserve(Joints.size());
		for (u32 Ind = 0; Ind < Joints.size(); Ind++)
		{
			Result.emplace_back(GetGlobaMatrix(Ind));
		}
		return Result;
	}
} // namespace Engine::Animation

namespace Engine::Transform
{
	auto Combine(const Comp::FTransform& Lhs, const Comp::FTransform& Rhs) -> Comp::FTransform
	{
		Comp::FTransform Result = {};

		// Scale: Parent.Scale * Child.Scale
		Result.Scale = Lhs.Scale * Rhs.Scale;
		// Rotation: Parent.Rotation * Child.Rotation
		Result.Rotation = Lhs.Rotation * Rhs.Rotation; // Assuming quaternion multiplication
		// Position: Parent.Position + Parent.Rotation * (Parent.Scale * Child.Position)
		Result.Position = Lhs.Position + Lhs.Rotation * (Lhs.Scale * Rhs.Position);

		return Result;
	}

	auto Inverse(const Comp::FTransform& In) -> Comp::FTransform
	{
		Comp::FTransform Result;

		Result.Rotation = glm::inverse(In.Rotation);

		Result.Scale.x = glm::abs(In.Scale.x) < glm::epsilon<glm::vec3::value_type>() ? 0.0f : 1.0f / In.Scale.x;
		Result.Scale.y = glm::abs(In.Scale.y) < glm::epsilon<glm::vec3::value_type>() ? 0.0f : 1.0f / In.Scale.y;
		Result.Scale.z = glm::abs(In.Scale.z) < glm::epsilon<glm::vec3::value_type>() ? 0.0f : 1.0f / In.Scale.z;

		Result.Position = Result.Rotation * (Result.Scale * (-In.Position));

		return Result;
	}

	auto Mix(const Comp::FTransform& Lhs, const Comp::FTransform& Rhs, f32 Time) -> Comp::FTransform
	{
		Comp::FTransform Result = {};

		auto RhsRot = Rhs.Rotation;
		if (glm::dot(Lhs.Rotation, Rhs.Rotation) < 0.0f)
		{
			RhsRot = -Rhs.Rotation;
		}

		Result.Position = glm::lerp(Lhs.Position, Rhs.Position, Time);
		Result.Rotation = glm::mix(Lhs.Rotation, RhsRot, Time);
		Result.Scale = glm::lerp(Lhs.Scale, Rhs.Scale, Time);

		return Result;
	}

	auto ToMatrix(const Comp::FTransform& In) -> glm::mat4
	{
		// Translation
		glm::mat4 translation = glm::translate(glm::identity<glm::mat4>(), In.Position);
		// Rotation (convert quaternion to matrix)
		glm::mat4 rotation = static_cast<glm::mat4>(In.Rotation);
		// Scale
		glm::mat4 scale = glm::scale(glm::identity<glm::mat4>(), In.Scale);
		// Final In: T * R * S
		return translation * rotation * scale;
	}

	auto ToTransform(const glm::mat4& Matrix) -> Comp::FTransform
	{
		Comp::FTransform Result = {};
		Result.Rotation = glm::normalize(glm::quat_cast(Matrix));
		Result.Position = glm::vec3(Matrix[3]);

		glm::vec3 Skew;
		glm::vec4 Perspective;
		glm::quat Rotation;
		glm::vec3 Position;
		glm::decompose(Matrix, Result.Scale, Rotation, Position, Skew, Perspective);

		return Result;
	}

	auto ToTransform(glm::dualquat const& DualQuat) -> Comp::FTransform
	{
		Comp::FTransform Result;
		Result.Rotation = DualQuat.real;
		glm::dualquat::part_type d = glm::conjugate(DualQuat.real) * (DualQuat.dual * 2.0f);
		Result.Position = glm::vec3(d.x, d.y, d.z);
		return Result;
	}

	auto ToDualQuat(Comp::FTransform const& Transform) -> glm::dualquat
	{
		glm::dualquat::part_type d(0.0f, Transform.Position);
		glm::dualquat::part_type qr = Transform.Rotation;
		glm::dualquat::part_type qd = qr * d * 0.5f;
		return glm::dualquat(qr, qd);
	}

	auto ToDualQuat(glm::mat4 const& Matrix) -> glm::dualquat
	{
		glm::quat rotation = glm::normalize(glm::quat_cast(Matrix));
		glm::vec3 translation = glm::vec3(Matrix[3]);
		glm::dualquat::part_type d(0.0f, translation);
		glm::dualquat::part_type qr = rotation;
		glm::dualquat::part_type qd = qr * d * 0.5f;
		return glm::normalize(glm::dualquat(qr, qd));
	}

	auto Point(const Comp::FTransform& In, const glm::vec3& Point) -> glm::vec3
	{
		return In.Position + In.Rotation * (In.Scale * Point);
	}

	auto Point(const glm::mat4& M, const glm::vec3& p) -> glm::vec3
	{
		// Treat as position (w = 1)
		glm::vec4 hp = glm::vec4(p, 1.0f);
		glm::vec4 result = M * hp;
		return glm::vec3(result);
	}

	auto Point(const glm::dualquat& dq, const glm::vec3& p) -> glm::vec3
	{
		glm::dualquat::part_type d = glm::conjugate(dq.real) * (dq.dual * 2.0f);
		auto t = glm::vec3(d.x, d.y, d.z);
		return dq.real * p + t;
	}

	auto Vector(const Comp::FTransform& In, const glm::vec3& Vector) -> glm::vec3
	{
		return In.Rotation * (In.Scale * Vector);
	}

	auto Vector(const glm::mat4& M, const glm::vec3& v) -> glm::vec3
	{
		return glm::vec3(M * glm::vec4(v, 0.0f));
	}

	auto Vector(const glm::dualquat& dq, const glm::vec3& v) -> glm::vec3
	{
		return dq.real * v;
	}
} // namespace Engine::Transform
