#version 450

layout (constant_id = 0) const float alpha_test_threshold = 0.;

// FIXME array size should be controlled by constant_id
layout(set=1,binding=0) uniform sampler2D textures[128];

layout(push_constant) uniform PUSH {
	int lightmap, texture;
} pc;

layout(location=0) in vec3 vPos;
layout(location=1) in vec2 vTexture0;
layout(location=2) in vec2 vLightmapUV;
layout(location=3) in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(textures[pc.texture], vTexture0) * vColor * texture(textures[pc.lightmap], vLightmapUV);
	if (outColor.a < alpha_test_threshold)
		discard;
}
