#extension GL_EXT_ray_tracing: require

#define GLSL
#include "ray_interop.h"
#undef GLSL

struct RayPayloadPrimary {
	vec4 hit_t;
	vec4 base_color_a;
};

#define PAYLOAD_LOCATION_PRIMARY 0