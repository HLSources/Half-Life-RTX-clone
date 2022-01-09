#extension GL_EXT_ray_tracing: require

#define GLSL
#include "ray_interop.h"
#undef GLSL

struct RayPayloadPrimary {
	PrimaryRay data;
};

#define PAYLOAD_LOCATION_PRIMARY 0
