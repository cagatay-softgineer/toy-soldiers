#pragma once

#include "physics/table_scene.h"
#include "render/camera.h"

#include "sokol_gfx.h"

namespace toy {

class Draw3D {
public:
	void init();
	void shutdown();
	// Begins a swapchain pass (caller ends pass + commits).
	// clearRgb optional room atmosphere clear color (0–1).
	void begin(int width, int height, const Camera& camera, float clearR = 0.07f, float clearG = 0.09f,
			   float clearB = 0.14f);
	void drawScene(const TableScene& scene);

private:
	void drawBox(const b3WorldTransform& xf, b3Vec3 halfExtents, b3Vec3 color, const float viewProj[16]);

	sg_pipeline pip_{};
	sg_bindings bind_{};
	sg_shader shd_{};
	sg_buffer vbuf_{};
	sg_buffer ibuf_{};
	bool ready_ = false;
	int width_ = 1;
	int height_ = 1;
	float viewProj_[16]{};
};

} // namespace toy
