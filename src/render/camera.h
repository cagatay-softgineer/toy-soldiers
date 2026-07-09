#pragma once

#include "box3d/math_functions.h"

namespace toy {

struct Camera {
	b3Vec3 target{ 0.0f, 0.15f, 0.0f };
	float distance = 4.2f;
	float yaw = 0.55f;   // radians
	float pitch = 0.55f; // radians
	float fovY = 50.0f * B3_DEG_TO_RAD;
	float nearZ = 0.05f;
	float farZ = 40.0f;

	void orbit(float dYaw, float dPitch);
	void zoom(float delta);
	// Gentle idle spin when idle (M5 polish).
	void tickIdle(float dt, bool enabled);
	b3Vec3 eye() const;

	// column-major 4x4 for GPU
	void viewMatrix(float out[16]) const;
	void projMatrix(float aspect, float out[16]) const;
	void viewProj(float aspect, float out[16]) const;
};

void mulMat4(const float a[16], const float b[16], float out[16]);
void identityMat4(float out[16]);

} // namespace toy
