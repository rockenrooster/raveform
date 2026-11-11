#version 450
// The last bloom accumulation (W/2). Besides A_1 = coarse weight x up(A_2) + w_1 x B_1 it gathers
// every low-resolution term the composite adds - pools, the afterglow excess and the background -
// each pre-divided by the bloom strength, so the composite reads one texture for all of them
// (design/look.md §7: scene + bloom + afterglow + background).
precision mediump int;
precision mediump float;

layout(set = 0, binding = 0) uniform mediump sampler2D coarse;
layout(set = 0, binding = 1) uniform mediump sampler2D level;
layout(set = 0, binding = 2) uniform mediump sampler2D pools;
layout(set = 0, binding = 4) uniform mediump sampler2D afterglow;
layout(set = 0, binding = 6) uniform mediump sampler2D background;

layout(push_constant) uniform Push {
    vec4 rot;
    vec4 p0;  // coarse weight, level weight, pools weight, afterglow weight
    vec4 p1;  // background weight
    vec4 p2;
} pc;

layout(location = 0) in highp vec2 vUv;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 c = pc.p0.z * texture(pools, vUv).rgb + pc.p1.x * texture(background, vUv).rgb;
    if (pc.p0.x != 0.0 || pc.p0.y != 0.0) c += pc.p0.x * texture(coarse, vUv).rgb + pc.p0.y * texture(level, vUv).rgb;
    if (pc.p0.w > 0.0) c += pc.p0.w * texture(afterglow, vUv).rgb;
    outColor = vec4(c, 1.0);
}
