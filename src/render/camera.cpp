#include "render/camera.h"

#include <cmath>
#include <cstring>

// std::sin

namespace toy {

void Camera::orbit(float dYaw, float dPitch)
{
	yaw += dYaw;
	pitch += dPitch;
	const float limit = 1.45f;
	if (pitch < 0.12f) {
		pitch = 0.12f;
	}
	if (pitch > limit) {
		pitch = limit;
	}
}

void Camera::zoom(float delta)
{
	distance *= (1.0f - delta * 0.1f);
	if (distance < 1.5f) {
		distance = 1.5f;
	}
	if (distance > 12.0f) {
		distance = 12.0f;
	}
}

void Camera::tickIdle(float dt, bool enabled)
{
	if (!enabled) {
		return;
	}
	yaw += dt * 0.08f;
	// Soft pitch bob
	const float bob = std::sin(yaw * 0.7f) * 0.01f;
	pitch = 0.52f + bob;
	if (pitch < 0.12f) {
		pitch = 0.12f;
	}
	if (pitch > 1.45f) {
		pitch = 1.45f;
	}
}

b3Vec3 Camera::eye() const
{
	const float cp = std::cos(pitch);
	const float sp = std::sin(pitch);
	const float cy = std::cos(yaw);
	const float sy = std::sin(yaw);
	return {
		target.x + distance * cp * sy,
		target.y + distance * sp,
		target.z + distance * cp * cy,
	};
}

void identityMat4(float out[16])
{
	std::memset(out, 0, 16 * sizeof(float));
	out[0] = out[5] = out[10] = out[15] = 1.0f;
}

void mulMat4(const float a[16], const float b[16], float out[16])
{
	float r[16];
	for (int c = 0; c < 4; ++c) {
		for (int row = 0; row < 4; ++row) {
			r[c * 4 + row] = a[0 * 4 + row] * b[c * 4 + 0] + a[1 * 4 + row] * b[c * 4 + 1] +
							 a[2 * 4 + row] * b[c * 4 + 2] + a[3 * 4 + row] * b[c * 4 + 3];
		}
	}
	std::memcpy(out, r, sizeof(r));
}

namespace {

void lookAt(const b3Vec3& eye, const b3Vec3& target, const b3Vec3& up, float out[16])
{
	b3Vec3 f = b3Normalize(b3Sub(target, eye));
	b3Vec3 s = b3Normalize(b3Cross(f, up));
	b3Vec3 u = b3Cross(s, f);

	identityMat4(out);
	out[0] = s.x;
	out[4] = s.y;
	out[8] = s.z;
	out[1] = u.x;
	out[5] = u.y;
	out[9] = u.z;
	out[2] = -f.x;
	out[6] = -f.y;
	out[10] = -f.z;
	out[12] = -b3Dot(s, eye);
	out[13] = -b3Dot(u, eye);
	out[14] = b3Dot(f, eye);
}

void perspective(float fovY, float aspect, float nearZ, float farZ, float out[16])
{
	const float f = 1.0f / std::tan(fovY * 0.5f);
	std::memset(out, 0, 16 * sizeof(float));
	out[0] = f / aspect;
	out[5] = f;
	out[10] = (farZ + nearZ) / (nearZ - farZ);
	out[11] = -1.0f;
	out[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
}

} // namespace

void Camera::viewMatrix(float out[16]) const
{
	lookAt(eye(), target, { 0.0f, 1.0f, 0.0f }, out);
}

void Camera::projMatrix(float aspect, float out[16]) const
{
	perspective(fovY, aspect, nearZ, farZ, out);
}

void Camera::viewProj(float aspect, float out[16]) const
{
	float v[16], p[16];
	viewMatrix(v);
	projMatrix(aspect, p);
	mulMat4(p, v, out);
}

} // namespace toy
