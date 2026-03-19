#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Platform.h>

namespace PlayerCtrl
{
	void Init(
		float maxHorizontalSpeed = 6.0f, float movementResponse = 0.2f, float jumpSpeed = 6.0f, float capsuleHalfHeight = 0.875f, float capsuleRadius = 0.25f);

	void Update(JPH::PhysicsSystem& Physics, const JPH::BodyID& PlayerBody, const FInputGamepad& Gamepad, float DeltaTime, float CameraYawDeg);
} // namespace PlayerCtrl
