#version 450
// Bloom accumulation on the way up: A_k = coarse weight x upsample(A_k+1) + w_k x B_k.
precision mediump int;
precision highp float;

layout(set = 0, binding = 0) uniform mediump sampler2D coarse;
layout(set = 0, binding = 1) uniform mediump sampler2D level;

layout(push_constant) uniform Push {
    vec4 rot;
    vec4 p0;  // coarse weight, level weight
    vec4 p1;
    vec4 p2;
} pc;

layout(location = 0) in vec2 vUv;
layout(location = 0) out mediump vec4 outColor;

void main() {
    mediump vec3 c = pc.p0.x * texture(coarse, vUv).rgb + pc.p0.y * texture(level, vUv).rgb;
    outColor = vec4(c, 1.0);
}
