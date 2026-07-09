#include "render/draw3d.h"

#include "box3d/box3d.h"

#include "sokol_glue.h"

#include <cstring>

namespace toy {

namespace {

// Unit cube: positions only. Scale via model matrix.
const float kCubeVerts[] = {
	// pos
	-1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1, // back
	-1, -1, 1,	1, -1, 1,  1, 1, 1,	 -1, 1, 1,	// front
};

const uint16_t kCubeIndices[] = {
	0, 1, 2, 0, 2, 3, // -Z
	4, 6, 5, 4, 7, 6, // +Z
	0, 4, 5, 0, 5, 1, // -Y
	2, 6, 7, 2, 7, 3, // +Y
	0, 3, 7, 0, 7, 4, // -X
	1, 5, 6, 1, 6, 2, // +X
};

#if defined(SOKOL_D3D11)
const char* kVsSrc = R"(
cbuffer vs_params : register(b0) {
  float4x4 mvp;
  float4 color;
};
struct vs_in { float3 pos : POSITION; };
struct vs_out { float4 color : COLOR0; float4 pos : SV_Position; };
vs_out main(vs_in inp) {
  vs_out outp;
  outp.pos = mul(mvp, float4(inp.pos, 1.0));
  outp.color = color;
  return outp;
}
)";
const char* kFsSrc = R"(
struct fs_in { float4 color : COLOR0; };
float4 main(fs_in inp) : SV_Target { return inp.color; }
)";
#elif defined(SOKOL_METAL)
const char* kVsSrc = R"(
#include <metal_stdlib>
using namespace metal;
struct params_t { float4x4 mvp; float4 color; };
struct vs_in { float3 pos [[attribute(0)]]; };
struct vs_out { float4 color [[user(color)]]; float4 pos [[position]]; };
vertex vs_out main0(vs_in in [[stage_in]], constant params_t& p [[buffer(0)]]) {
  vs_out out;
  out.pos = p.mvp * float4(in.pos, 1.0);
  out.color = p.color;
  return out;
}
)";
const char* kFsSrc = R"(
#include <metal_stdlib>
using namespace metal;
struct fs_in { float4 color [[user(color)]]; };
fragment float4 main0(fs_in in [[stage_in]]) { return in.color; }
)";
#else
const char* kVsSrc = R"(
#version 410
uniform vec4 vs_params[5];
layout(location=0) in vec3 position;
out vec4 color;
void main() {
  mat4 mvp = mat4(vs_params[0], vs_params[1], vs_params[2], vs_params[3]);
  color = vs_params[4];
  gl_Position = mvp * vec4(position, 1.0);
}
)";
const char* kFsSrc = R"(
#version 410
in vec4 color;
out vec4 frag_color;
void main() { frag_color = color; }
)";
#endif

struct VsParams {
	float mvp[16];
	float color[4];
};

void quatToModel(const b3WorldTransform& xf, b3Vec3 halfExtents, float model[16])
{
	const b3Matrix3 r = b3MakeMatrixFromQuat(xf.q);
	// columns: scaled axes + translation
	model[0] = r.cx.x * halfExtents.x;
	model[1] = r.cx.y * halfExtents.x;
	model[2] = r.cx.z * halfExtents.x;
	model[3] = 0.0f;
	model[4] = r.cy.x * halfExtents.y;
	model[5] = r.cy.y * halfExtents.y;
	model[6] = r.cy.z * halfExtents.y;
	model[7] = 0.0f;
	model[8] = r.cz.x * halfExtents.z;
	model[9] = r.cz.y * halfExtents.z;
	model[10] = r.cz.z * halfExtents.z;
	model[11] = 0.0f;
	model[12] = static_cast<float>(xf.p.x);
	model[13] = static_cast<float>(xf.p.y);
	model[14] = static_cast<float>(xf.p.z);
	model[15] = 1.0f;
}

} // namespace

void Draw3D::init()
{
	{
		sg_buffer_desc bd = {};
		bd.data = SG_RANGE(kCubeVerts);
		bd.label = "cube-vertices";
		vbuf_ = sg_make_buffer(&bd);
	}
	{
		sg_buffer_desc bd = {};
		bd.usage.index_buffer = true;
		bd.data = SG_RANGE(kCubeIndices);
		bd.label = "cube-indices";
		ibuf_ = sg_make_buffer(&bd);
	}

	sg_shader_desc sd = {};
	sd.label = "solid-cube";
	sd.attrs[0].base_type = SG_SHADERATTRBASETYPE_FLOAT;
#if defined(SOKOL_D3D11)
	sd.vertex_func.source = kVsSrc;
	sd.vertex_func.d3d11_target = "vs_5_0";
	sd.fragment_func.source = kFsSrc;
	sd.fragment_func.d3d11_target = "ps_5_0";
	sd.attrs[0].hlsl_sem_name = "POSITION";
	sd.attrs[0].hlsl_sem_index = 0;
	sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
	sd.uniform_blocks[0].size = sizeof(VsParams);
	sd.uniform_blocks[0].hlsl_register_b_n = 0;
#elif defined(SOKOL_METAL)
	sd.vertex_func.source = kVsSrc;
	sd.vertex_func.entry = "main0";
	sd.fragment_func.source = kFsSrc;
	sd.fragment_func.entry = "main0";
	sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
	sd.uniform_blocks[0].size = sizeof(VsParams);
	sd.uniform_blocks[0].msl_buffer_n = 0;
#else
	sd.vertex_func.source = kVsSrc;
	sd.fragment_func.source = kFsSrc;
	sd.attrs[0].glsl_name = "position";
	sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
	sd.uniform_blocks[0].size = sizeof(VsParams);
	sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
	sd.uniform_blocks[0].glsl_uniforms[0].array_count = 5;
	sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "vs_params";
#endif
	shd_ = sg_make_shader(&sd);

	sg_pipeline_desc pd = {};
	pd.shader = shd_;
	pd.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
	pd.index_type = SG_INDEXTYPE_UINT16;
	pd.cull_mode = SG_CULLMODE_NONE; // unit cube index order is mixed; disable cull for jam
	pd.depth.write_enabled = true;
	pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
	pd.label = "cube-pipeline";
	pip_ = sg_make_pipeline(&pd);

	bind_ = {};
	bind_.vertex_buffers[0] = vbuf_;
	bind_.index_buffer = ibuf_;
	ready_ = true;
}

void Draw3D::shutdown()
{
	if (!ready_) {
		return;
	}
	sg_destroy_pipeline(pip_);
	sg_destroy_shader(shd_);
	sg_destroy_buffer(ibuf_);
	sg_destroy_buffer(vbuf_);
	ready_ = false;
}

void Draw3D::begin(int width, int height, const Camera& camera, float clearR, float clearG, float clearB)
{
	width_ = width > 0 ? width : 1;
	height_ = height > 0 ? height : 1;
	const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
	camera.viewProj(aspect, viewProj_);

	sg_pass pass = {};
	pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
	pass.action.colors[0].clear_value = { clearR, clearG, clearB, 1.0f };
	pass.action.depth.load_action = SG_LOADACTION_CLEAR;
	pass.action.depth.clear_value = 1.0f;
	pass.swapchain = sglue_swapchain();
	sg_begin_pass(&pass);
	sg_apply_pipeline(pip_);
	sg_apply_bindings(&bind_);
}

void Draw3D::drawBox(const b3WorldTransform& xf, b3Vec3 halfExtents, b3Vec3 color, const float viewProj[16])
{
	float model[16];
	quatToModel(xf, halfExtents, model);
	float mvp[16];
	mulMat4(viewProj, model, mvp);

	VsParams params{};
	std::memcpy(params.mvp, mvp, sizeof(mvp));
	params.color[0] = color.x;
	params.color[1] = color.y;
	params.color[2] = color.z;
	params.color[3] = 1.0f;
	const sg_range ub = SG_RANGE(params);
	sg_apply_uniforms(0, &ub);
	sg_draw(0, 36, 1);
}

void Draw3D::drawScene(const TableScene& scene)
{
	for (const BodyVisual& v : scene.visuals()) {
		if (!v.valid || !b3Body_IsValid(v.body)) {
			continue;
		}
		const b3WorldTransform xf = b3Body_GetTransform(v.body);
		drawBox(xf, v.halfExtents, v.color, viewProj_);
	}
}

} // namespace toy
