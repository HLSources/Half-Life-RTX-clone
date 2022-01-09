#version 460 core
#extension GL_GOOGLE_include_directive : require

#include "ray_primary_common.glsl"

layout(location = PAYLOAD_LOCATION_PRIMARY) rayPayloadInEXT RayPayloadPrimary payload;

void main() {
	payload.data.pos_t = vec4(0., 0., 0., 1e6);
	payload.data.base_color = vec4(1., 0., 1., 0.);
	payload.data.normal_geometry = vec3(0.f);
	payload.data.normal_shading = vec3(0.f);
	payload.data.roughness = 1.f;
	payload.data.metalness = 0.f;
}
