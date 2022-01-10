#version 460 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "ray_primary_common.glsl"

#include "ray_kusochki.glsl"

layout(constant_id = 6) const uint MAX_TEXTURES = 4096;
layout(set = 0, binding = 6) uniform sampler2D textures[MAX_TEXTURES];
layout(set = 0, binding = 2) uniform UBO { UniformBuffer ubo; };

layout(location = PAYLOAD_LOCATION_PRIMARY) rayPayloadInEXT RayPayloadPrimary payload;
hitAttributeEXT vec2 bary;

#include "rt_geometry.glsl"

vec4 sampleTexture(uint tex_index, vec2 uv, vec4 uv_lods) {
	return textureGrad(textures[nonuniformEXT(tex_index)], uv, uv_lods.xy, uv_lods.zw);
}

void main() {
	const Geometry geom = readHitGeometry();

	//payload.data.pos_t = vec4(geom.pos, gl_HitTEXT);
	/* payload.data.normal_geometry = geom.normal_geometry; */
	/* payload.data.normal_shading = geom.normal_shading; */
  /*  */
	/* // FIXME */
	/* payload.data.roughness = 1.f; */
	/* payload.data.metalness = 0.f; */

	const Kusok kusok = kusochki[geom.kusok_index];
	const uint tex_base_color = kusok.tex_base_color;

	if ((tex_base_color & KUSOK_MATERIAL_FLAG_SKYBOX) != 0) {
		// FIXME read skybox
		payload.data.base_color = vec4(1.,0.,1.,1.);
	} else {
		payload.data.base_color = sampleTexture(tex_base_color, geom.uv, geom.uv_lods) * kusok.color;
	}
}
