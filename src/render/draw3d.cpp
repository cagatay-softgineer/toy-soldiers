#include "render/draw3d.h"

#include "box3d/box3d.h"

#include "sokol_glue.h"

#include <cmath>
#include <cstring>

namespace toy {

namespace {

// Unit cube with per-face normals (24 verts) — needed for v0.9 #118 lighting.
// Layout: pos.xyz, nrm.xyz
const float kCubeVerts[] = {
	// -Z
	-1, -1, -1, 0, 0, -1, 1, -1, -1, 0, 0, -1, 1, 1, -1, 0, 0, -1, -1, 1, -1, 0, 0, -1,
	// +Z
	-1, -1, 1, 0, 0, 1, 1, -1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, -1, 1, 1, 0, 0, 1,
	// -Y
	-1, -1, -1, 0, -1, 0, 1, -1, -1, 0, -1, 0, 1, -1, 1, 0, -1, 0, -1, -1, 1, 0, -1, 0,
	// +Y
	-1, 1, -1, 0, 1, 0, 1, 1, -1, 0, 1, 0, 1, 1, 1, 0, 1, 0, -1, 1, 1, 0, 1, 0,
	// -X
	-1, -1, -1, -1, 0, 0, -1, 1, -1, -1, 0, 0, -1, 1, 1, -1, 0, 0, -1, -1, 1, -1, 0, 0,
	// +X
	1, -1, -1, 1, 0, 0, 1, 1, -1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, -1, 1, 1, 0, 0,
};

const uint16_t kCubeIndices[] = {
	0,  2,  1,  0,  3,  2,  // -Z
	4,  5,  6,  4,  6,  7,  // +Z
	8,  9,  10, 8,  10, 11, // -Y
	12, 14, 13, 12, 15, 14, // +Y
	16, 17, 18, 16, 18, 19, // -X
	20, 22, 21, 20, 23, 22, // +X
};

#if defined(SOKOL_D3D11)
const char* kVsSrc = R"(
cbuffer vs_params : register(b0) {
  float4x4 view_proj;
  float4 light; // xyz = dir to light (normalized), w = ambient
};
struct vs_in {
  float3 pos : POSITION;
  float3 nrm : NORMAL;
  float4 m0 : TEXCOORD0;
  float4 m1 : TEXCOORD1;
  float4 m2 : TEXCOORD2;
  float4 m3 : TEXCOORD3;
  float4 icolor : COLOR0;
};
struct vs_out { float4 color : COLOR0; float4 pos : SV_Position; };
vs_out main(vs_in inp) {
  vs_out outp;
  // m0..m3 are the model matrix COLUMNS -> float4x4(rows) builds the transpose,
  // so mul(row_vector, M^T) == M * v.
  float4x4 modelT = float4x4(inp.m0, inp.m1, inp.m2, inp.m3);
  float4 world = mul(float4(inp.pos, 1.0), modelT);
  float3 n = normalize(mul(inp.nrm, (float3x3)modelT));
  float ndl = max(dot(n, light.xyz), 0.0);
  float lum = light.w + (1.0 - light.w) * ndl;
  // Cheap plastic sheen: extra pop on strongly lit faces.
  lum += 0.15 * pow(ndl, 8.0);
  outp.color = float4(inp.icolor.rgb * lum, inp.icolor.a);
  outp.pos = mul(view_proj, world);
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
struct params_t { float4x4 view_proj; float4 light; };
struct vs_in {
  float3 pos [[attribute(0)]];
  float3 nrm [[attribute(1)]];
  float4 m0 [[attribute(2)]];
  float4 m1 [[attribute(3)]];
  float4 m2 [[attribute(4)]];
  float4 m3 [[attribute(5)]];
  float4 icolor [[attribute(6)]];
};
struct vs_out { float4 color [[user(color)]]; float4 pos [[position]]; };
vertex vs_out main0(vs_in in [[stage_in]], constant params_t& p [[buffer(0)]]) {
  vs_out out;
  float4x4 model = float4x4(in.m0, in.m1, in.m2, in.m3); // columns
  float4 world = model * float4(in.pos, 1.0);
  float3 n = normalize((float3x3(model[0].xyz, model[1].xyz, model[2].xyz)) * in.nrm);
  float ndl = max(dot(n, p.light.xyz), 0.0);
  float lum = p.light.w + (1.0 - p.light.w) * ndl + 0.15 * pow(ndl, 8.0);
  out.color = float4(in.icolor.rgb * lum, in.icolor.a);
  out.pos = p.view_proj * world;
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
layout(location=1) in vec3 normal;
layout(location=2) in vec4 im0;
layout(location=3) in vec4 im1;
layout(location=4) in vec4 im2;
layout(location=5) in vec4 im3;
layout(location=6) in vec4 icolor;
out vec4 color;
void main() {
  mat4 view_proj = mat4(vs_params[0], vs_params[1], vs_params[2], vs_params[3]);
  vec4 light = vs_params[4];
  mat4 model = mat4(im0, im1, im2, im3); // GLSL mat4 ctor takes columns
  vec4 world = model * vec4(position, 1.0);
  vec3 n = normalize(mat3(model) * normal);
  float ndl = max(dot(n, light.xyz), 0.0);
  float lum = light.w + (1.0 - light.w) * ndl + 0.15 * pow(ndl, 8.0);
  color = vec4(icolor.rgb * lum, icolor.a);
  gl_Position = view_proj * world;
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
	float viewProj[16];
	float light[4]; // xyz dir-to-light, w ambient
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

void axisModel(b3Vec3 center, b3Vec3 halfExtents, float model[16])
{
	std::memset(model, 0, sizeof(float) * 16);
	model[0] = halfExtents.x;
	model[5] = halfExtents.y;
	model[10] = halfExtents.z;
	model[12] = center.x;
	model[13] = center.y;
	model[14] = center.z;
	model[15] = 1.0f;
}

} // namespace

void Draw3D::init()
{
	instances_.reserve(kMaxInstances);
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
	{
		// v0.9 #117: per-instance model matrix + color, streamed each frame.
		sg_buffer_desc bd = {};
		bd.usage.vertex_buffer = true;
		bd.usage.stream_update = true;
		bd.size = sizeof(Instance) * kMaxInstances;
		bd.label = "cube-instances";
		instbuf_ = sg_make_buffer(&bd);
	}

	sg_shader_desc sd = {};
	sd.label = "lit-instanced-cube";
	for (int i = 0; i < 7; ++i) {
		sd.attrs[i].base_type = SG_SHADERATTRBASETYPE_FLOAT;
	}
#if defined(SOKOL_D3D11)
	sd.vertex_func.source = kVsSrc;
	sd.vertex_func.d3d11_target = "vs_5_0";
	sd.fragment_func.source = kFsSrc;
	sd.fragment_func.d3d11_target = "ps_5_0";
	sd.attrs[0].hlsl_sem_name = "POSITION";
	sd.attrs[1].hlsl_sem_name = "NORMAL";
	sd.attrs[2].hlsl_sem_name = "TEXCOORD";
	sd.attrs[2].hlsl_sem_index = 0;
	sd.attrs[3].hlsl_sem_name = "TEXCOORD";
	sd.attrs[3].hlsl_sem_index = 1;
	sd.attrs[4].hlsl_sem_name = "TEXCOORD";
	sd.attrs[4].hlsl_sem_index = 2;
	sd.attrs[5].hlsl_sem_name = "TEXCOORD";
	sd.attrs[5].hlsl_sem_index = 3;
	sd.attrs[6].hlsl_sem_name = "COLOR";
	sd.attrs[6].hlsl_sem_index = 0;
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
	sd.attrs[1].glsl_name = "normal";
	sd.attrs[2].glsl_name = "im0";
	sd.attrs[3].glsl_name = "im1";
	sd.attrs[4].glsl_name = "im2";
	sd.attrs[5].glsl_name = "im3";
	sd.attrs[6].glsl_name = "icolor";
	sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
	sd.uniform_blocks[0].size = sizeof(VsParams);
	sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
	sd.uniform_blocks[0].glsl_uniforms[0].array_count = 5;
	sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "vs_params";
#endif
	shd_ = sg_make_shader(&sd);

	sg_pipeline_desc pd = {};
	pd.shader = shd_;
	// Buffer 0: mesh (pos + normal). Buffer 1: per-instance data.
	pd.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE;
	pd.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
	pd.layout.attrs[0].buffer_index = 0;
	pd.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT3;
	pd.layout.attrs[1].buffer_index = 0;
	for (int i = 2; i <= 6; ++i) {
		pd.layout.attrs[i].format = SG_VERTEXFORMAT_FLOAT4;
		pd.layout.attrs[i].buffer_index = 1;
	}
	pd.index_type = SG_INDEXTYPE_UINT16;
	pd.cull_mode = SG_CULLMODE_NONE; // mixed winding tolerated for jam
	pd.depth.write_enabled = true;
	pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
	pd.label = "cube-pipeline";
	pip_ = sg_make_pipeline(&pd);

	bind_ = {};
	bind_.vertex_buffers[0] = vbuf_;
	bind_.vertex_buffers[1] = instbuf_;
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
	sg_destroy_buffer(instbuf_);
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
	instances_.clear();
}

void Draw3D::pushBox(const b3WorldTransform& xf, b3Vec3 halfExtents, b3Vec3 color)
{
	if (static_cast<int>(instances_.size()) >= kMaxInstances) {
		return;
	}
	Instance inst;
	quatToModel(xf, halfExtents, inst.model);
	inst.color[0] = color.x;
	inst.color[1] = color.y;
	inst.color[2] = color.z;
	inst.color[3] = 1.0f;
	instances_.push_back(inst);
}

void Draw3D::pushAxisBox(b3Vec3 center, b3Vec3 halfExtents, b3Vec3 color)
{
	if (static_cast<int>(instances_.size()) >= kMaxInstances) {
		return;
	}
	Instance inst;
	axisModel(center, halfExtents, inst.model);
	inst.color[0] = color.x;
	inst.color[1] = color.y;
	inst.color[2] = color.z;
	inst.color[3] = 1.0f;
	instances_.push_back(inst);
}

void Draw3D::flush()
{
	if (instances_.empty()) {
		return;
	}
	sg_apply_pipeline(pip_);
	sg_apply_bindings(&bind_);

	VsParams params{};
	std::memcpy(params.viewProj, viewProj_, sizeof(viewProj_));
	// v0.9 #118: warm key light from above-left, toy-room ambient.
	const float lx = 0.45f, ly = 0.80f, lz = 0.30f;
	const float len = std::sqrt(lx * lx + ly * ly + lz * lz);
	params.light[0] = lx / len;
	params.light[1] = ly / len;
	params.light[2] = lz / len;
	params.light[3] = 0.42f; // ambient
	const sg_range ub = SG_RANGE(params);
	sg_apply_uniforms(0, &ub);

	sg_range data;
	data.ptr = instances_.data();
	data.size = instances_.size() * sizeof(Instance);
	sg_update_buffer(instbuf_, &data);
	// v0.9 #117: the whole table in one instanced call.
	sg_draw(0, 36, static_cast<int>(instances_.size()));
}

void Draw3D::drawScene(const TableScene& scene, const RenderFx& fx)
{
	const b3Vec3 shadowCol{ fx.shadowR, fx.shadowG, fx.shadowB };

	for (const BodyVisual& v : scene.visuals()) {
		if (!v.valid || !b3Body_IsValid(v.body)) {
			continue;
		}
		b3WorldTransform xf = b3Body_GetTransform(v.body);

		// v0.9 #124: soldiers idle-bob; the winner's platoon cheers.
		if (v.kind == BodyVisual::Kind::Soldier && !fx.reducedMotion) {
			const float phase = static_cast<float>(v.playerId) * 1.7f + static_cast<float>(xf.p.x + xf.p.z) * 3.1f;
			float lift = 0.0f;
			if (v.playerId == fx.winnerSeat) {
				lift = std::fabs(std::sin(fx.time * 6.0f + phase)) * 0.05f;
			} else if (v.playerId == fx.activeSeat) {
				lift = std::fabs(std::sin(fx.time * 3.0f + phase)) * 0.012f;
			} else {
				lift = std::sin(fx.time * 2.0f + phase) * 0.004f + 0.004f;
			}
			xf.p.y = xf.p.y + lift;
		}
		pushBox(xf, v.halfExtents, v.color);

		// v0.9 #119: blob shadow under towers + soldiers.
		if ((v.kind == BodyVisual::Kind::Tower || v.kind == BodyVisual::Kind::Soldier) &&
			static_cast<float>(xf.p.y) > -0.2f) {
			pushAxisBox({ static_cast<float>(xf.p.x), 0.0028f, static_cast<float>(xf.p.z) },
						{ v.halfExtents.x * 1.25f, 0.0015f, v.halfExtents.z * 1.25f }, shadowCol);
		}
	}

	// v0.9 #121: pulsing ground ring around the selected target tower.
	if (fx.outlineSeat >= 0 && fx.outlineSeat < kMaxPlayers) {
		const b3Vec3 seat = TableScene::seatPosition(fx.outlineSeat);
		const float pulse = fx.reducedMotion ? 0.0f : std::sin(fx.time * 6.0f) * 0.02f;
		const float r = 0.30f + pulse;
		const float t = 0.02f;
		const b3Vec3 ring{ 0.98f, 0.78f, 0.30f };
		pushAxisBox({ seat.x - r, 0.006f, seat.z }, { t, 0.004f, r }, ring);
		pushAxisBox({ seat.x + r, 0.006f, seat.z }, { t, 0.004f, r }, ring);
		pushAxisBox({ seat.x, 0.006f, seat.z - r }, { r, 0.004f, t }, ring);
		pushAxisBox({ seat.x, 0.006f, seat.z + r }, { r, 0.004f, t }, ring);
	}

	// v0.9 #127/#128: particles as tiny cubes.
	for (const FxParticle& p : scene.particles()) {
		pushAxisBox(p.pos, { p.size, p.size, p.size }, p.color);
	}

	flush();
}

} // namespace toy
