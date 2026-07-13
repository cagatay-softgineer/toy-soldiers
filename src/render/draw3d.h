#pragma once

#include "physics/table_scene.h"
#include "render/camera.h"

#include "sokol_gfx.h"

namespace toy {

// v0.9 render juice knobs — computed by main each frame (#121, #124, #128).
struct RenderFx {
	float time = 0.0f;       // seconds, for bobs/pulses
	int outlineSeat = -1;    // selected target tower gets a pulsing ground ring (#121)
	int activeSeat = -1;     // subtle lift on the acting seat's soldiers
	int winnerSeat = -1;     // cheering bob on victory (#124)
	bool reducedMotion = false;
	bool detailedSoldiers = true; // v1.2 #123: articulated toy figures (render-only)
	float shadowR = 0.10f, shadowG = 0.10f, shadowB = 0.10f; // blob shadow tint (#119)
};

// v0.9 #117/#118: one instanced draw call for every cube, with directional
// "plastic" shading (N·L + ambient) baked into the shader.
class Draw3D {
public:
	void init();
	void shutdown();
	// Begins a swapchain pass (caller ends pass + commits).
	// clearRgb optional room atmosphere clear color (0–1).
	void begin(int width, int height, const Camera& camera, float clearR = 0.07f, float clearG = 0.09f,
			   float clearB = 0.14f);
	void drawScene(const TableScene& scene) { drawScene(scene, RenderFx{}); }
	void drawScene(const TableScene& scene, const RenderFx& fx);

private:
	struct Instance {
		float model[16];
		float color[4];
	};

	void pushBox(const b3WorldTransform& xf, b3Vec3 halfExtents, b3Vec3 color);
	void pushDetailedSoldier(const b3WorldTransform& xf, b3Vec3 half, b3Vec3 color);
	void pushAxisBox(b3Vec3 center, b3Vec3 halfExtents, b3Vec3 color);
	void flush();

	static constexpr int kMaxInstances = 4096;

	sg_pipeline pip_{};
	sg_bindings bind_{};
	sg_shader shd_{};
	sg_buffer vbuf_{};
	sg_buffer ibuf_{};
	sg_buffer instbuf_{};
	bool ready_ = false;
	int width_ = 1;
	int height_ = 1;
	float viewProj_[16]{};
	std::vector<Instance> instances_;
};

} // namespace toy
