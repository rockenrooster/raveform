#version 450
// One level of the bloom pyramid: halve the resolution and blur in one pass. Four bilinear taps at
// (+-a, +-a) source texels around the destination texel centre (a source texel corner); the offsets are
// solved on the CPU so that the stack of levels reproduces the Gaussian sigmas of design/look.json.
// The first level also adds the quarter-resolution pools, so they feed the bloom (look.md §3).
precision mediump int;
precision highp float;

layout(set = 0, binding = 0) uniform mediump sampler2D src;
layout(set = 0, binding = 1) uniform mediump sampler2D pools;

layout(push_constant) uniform Push {
    vec4 rot;
    vec4 p0;  // tap offset x, y (uv), pool weight, unused
    vec4 p1;
    vec4 p2;
} pc;

layout(location = 0) in vec2 vUv;
layout(location = 0) out mediump vec4 outColor;

void main() {
    vec2 o = pc.p0.xy;
    mediump vec3 s = texture(src, vUv + vec2(-o.x, -o.y)).rgb + texture(src, vUv + vec2(o.x, -o.y)).rgb +
                     texture(src, vUv + vec2(-o.x, o.y)).rgb + texture(src, vUv + vec2(o.x, o.y)).rgb;
    s *= 0.25;
    if (pc.p0.z > 0.0) s += pc.p0.z * texture(pools, vUv).rgb;
    outColor = vec4(s, 1.0);
}
