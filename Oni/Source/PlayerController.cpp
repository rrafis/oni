#include "PlayerController.h"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <cmath>
#include <cstdio>

namespace PlayerCtrl
{
	static float MaxHorizontalSpeed = 6.0f;
	static float MovementResponse = 0.2f; // 0..1
	static float JumpSpeed = 6.0f;
	static float CapsuleHalfHeight = 0.875f;
	static float CapsuleRadius = 0.25f;
	static float StartAboveBottom = 0.02f;
	static float CheckDistance = 0.04f;
	static float Deadzone = 0.01f;
	static bool Initialized = false;

	void Init(float maxHorizontalSpeed, float movementResponse, float jumpSpeed, float capsuleHalfHeight, float capsuleRadius)
	{
		MaxHorizontalSpeed = maxHorizontalSpeed;
		MovementResponse = movementResponse;
		JumpSpeed = jumpSpeed;
		CapsuleHalfHeight = capsuleHalfHeight;
		CapsuleRadius = capsuleRadius;
		Initialized = true;
	}

	void Update(JPH::PhysicsSystem& Physics, const JPH::BodyID& PlayerBody, const FInputGamepad& Gamepad, float DeltaTime, float CameraYawDeg)
	{
		if (!Initialized)
			Init();

		auto& BodyInterface = Physics.GetBodyInterface();

		float lx = Gamepad.GamepadThumbstickLeft.X;
		float lz = Gamepad.GamepadThumbstickLeft.Y;
		if (Gamepad.GamepadDpadUp.bPressed)
			lz = 1.0f;
		if (Gamepad.GamepadDpadDown.bPressed)
			lz = -1.0f;
		if (Gamepad.GamepadDpadRight.bPressed)
			lx = 1.0f;
		if (Gamepad.GamepadDpadLeft.bPressed)
			lx = -1.0f;

		// transform input by camera yaw so movement is camera-relative
		float len = std::sqrt(lx * lx + lz * lz);
		float desiredX = 0.0f;
		float desiredZ = 0.0f;
		if (len > 0.0f)
		{
			// normalize input direction
			lx /= len;
			lz /= len;

			float yawRad = CameraYawDeg * (3.14159265358979323846f / 180.0f);
			// camera forward (world) = (sin(yaw), 0, cos(yaw)) -> forward when yaw=0 is +Z
			float fx = std::sinf(yawRad);
			float fz = std::cosf(yawRad);
			// right = (cos(yaw), 0, -sin(yaw))
			float rx = std::cosf(yawRad);
			float rz = -std::sinf(yawRad);

			// input in camera space: lx = right, lz = forward
			float worldX = lx * rx + lz * fx;
			float worldZ = lx * rz + lz * fz;

			// desired horizontal velocity in world space
			desiredX = worldX * MaxHorizontalSpeed;
			desiredZ = worldZ * MaxHorizontalSpeed;
		}

		// current velocity
		JPH::Vec3 currentVel = BodyInterface.GetLinearVelocity(PlayerBody);

		// compute deltaV (XZ)
		float deltaX = desiredX - currentVel.GetX();
		float deltaZ = desiredZ - currentVel.GetZ();

		// get mass
		float mass = 1.0f;
		{
			JPH::BodyLockRead lock(Physics.GetBodyLockInterface(), PlayerBody);
			const JPH::Body& b = lock.GetBody();
			float invMass = b.GetMotionProperties()->GetInverseMass();
			if (invMass > 0.0f)
				mass = 1.0f / invMass;
		}

		// apply horizontal as a force ( smoother than per-frame impulses ) scaled by response (deadzone avoids micro adjustments)
		if (DeltaTime > 0.0f && (std::fabs(deltaX) > Deadzone || std::fabs(deltaZ) > Deadzone))
		{
			JPH::Vec3 desiredImpulse(deltaX * mass * MovementResponse, 0.0f, deltaZ * mass * MovementResponse);
			// Clamp impulse magnitude to avoid large corrections that cause instability
			float maxImpulse = mass * MaxHorizontalSpeed * MovementResponse * 0.75f; // tuned cap
			float impLen = desiredImpulse.Length();
			if (impLen > maxImpulse && impLen > 0.0f)
			{
				desiredImpulse = desiredImpulse * (maxImpulse / impLen);
			}
			// Convert impulse to force for this timestep: F = impulse / dt
			JPH::Vec3 force = desiredImpulse / DeltaTime;
			BodyInterface.AddForce(PlayerBody, force);
		}

		// Ground test: cast a short ray just below capsule bottom, ignore self
		bool grounded = false;
		{
			JPH::RVec3 pos = BodyInterface.GetPosition(PlayerBody);
			JPH::RVec3 bottom = pos - JPH::RVec3(0.0, CapsuleHalfHeight + CapsuleRadius, 0.0);
			JPH::RVec3 origin = bottom + JPH::RVec3(0.0, StartAboveBottom, 0.0);
			JPH::RVec3 dir = JPH::RVec3(0.0, -(StartAboveBottom + CheckDistance), 0.0);
			JPH::RRayCast ray{ origin, dir };
			JPH::RayCastResult hit;
			JPH::IgnoreSingleBodyFilter body_filter(PlayerBody);
			grounded = Physics.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, body_filter);
			// (Debug prints removed to avoid spamming and causing jitter)
		}

		// Jump rising-edge detection: store state statically per-process
		static bool sWasJumpDown = false;
		bool jumpDown = Gamepad.GamepadA.bPressed;
		if (jumpDown && !sWasJumpDown && grounded)
		{
			// ensure vertical velocity is low to avoid double-jump during upward motion
			JPH::Vec3 cur = BodyInterface.GetLinearVelocity(PlayerBody);
			if (cur.GetY() < 0.5f)
			{
				JPH::Vec3 jumpImpulse = JPH::Vec3(0.0f, JumpSpeed * mass, 0.0f);
				BodyInterface.AddImpulse(PlayerBody, jumpImpulse);
			}
		}
		sWasJumpDown = jumpDown;
	}

} // namespace PlayerCtrl
