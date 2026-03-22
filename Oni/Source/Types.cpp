#include "Types.h"

namespace Engine::Transform
{
	auto Combine(const FTransform& Lhs, const FTransform& Rhs) -> FTransform
	{
		FTransform Result = {};

		// Scale: Parent.Scale * Child.Scale
		Result.Scale = Lhs.Scale * Rhs.Scale;
		// Rotation: Parent.Rotation * Child.Rotation
		Result.Rotation = Lhs.Rotation * Rhs.Rotation; // Assuming quaternion multiplication
		// Position: Parent.Position + Parent.Rotation * (Parent.Scale * Child.Position)
		Result.Position = Lhs.Position + Lhs.Rotation * (Lhs.Scale * Rhs.Position);

		return Result;
	}

	auto Inverse(const FTransform& In) -> FTransform
	{
		FTransform Result;

		Result.Rotation = glm::inverse(In.Rotation);

		Result.Scale.x = glm::abs(In.Scale.x) < glm::epsilon<glm::vec3::value_type>() ? 0.0f : 1.0f / In.Scale.x;
		Result.Scale.y = glm::abs(In.Scale.y) < glm::epsilon<glm::vec3::value_type>() ? 0.0f : 1.0f / In.Scale.y;
		Result.Scale.z = glm::abs(In.Scale.z) < glm::epsilon<glm::vec3::value_type>() ? 0.0f : 1.0f / In.Scale.z;

		Result.Position = Result.Rotation * (Result.Scale * (-In.Position));

		return Result;
	}

	auto Mix(const FTransform& Lhs, const FTransform& Rhs, f32 Time) -> FTransform
	{
		FTransform Result = {};

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

	auto ToMatrix(const FTransform& In) -> glm::mat4
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

	auto ToTransform(const glm::mat4& Matrix) -> FTransform
	{
		FTransform Result = {};
		Result.Rotation = glm::normalize(glm::quat_cast(Matrix));
		Result.Position = glm::vec3(Matrix[3]);

		glm::vec3 Skew;
		glm::vec4 Perspective;
		glm::quat Rotation;
		glm::vec3 Position;
		glm::decompose(Matrix, Result.Scale, Rotation, Position, Skew, Perspective);

		return Result;
	}

	auto ToTransform(glm::dualquat const& DualQuat) -> FTransform
	{
		FTransform Result;
		Result.Rotation = DualQuat.real;
		glm::dualquat::part_type d = glm::conjugate(DualQuat.real) * (DualQuat.dual * 2.0f);
		Result.Position = glm::vec3(d.x, d.y, d.z);
		return Result;
	}

	auto ToDualQuat(FTransform const& Transform) -> glm::dualquat
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

	auto Point(const FTransform& In, const glm::vec3& Point) -> glm::vec3
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

	auto Vector(const FTransform& In, const glm::vec3& Vector) -> glm::vec3
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
