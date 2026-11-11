#version 450
// Distance to the segment in pixels gives a hard core with 1 px anti-aliasing plus an optional soft
// skirt. Output is linear light for additive blending (ONE, ONE). Vulkan GLSL defaults to highp;
// colour maths is marked mediump (RelaxedPrecision), distances stay highp.
precision mediump int;
precision highp float;

layout(location = 0) in mediump vec3 vColor;
layout(location = 1) in vec2 vLocal;
layout(location = 2) flat in vec3 vShape;   // length, radius, skirt

layout(push_constant) uniform Push {
    vec4 view;
    vec4 rot;
    vec4 misc;
} pc;

layout(location = 0) out mediump vec4 outColor;

mediump vec3 linearToSrgb(mediump vec3 c) {
    mediump vec3 lo = c * 12.92;
    mediump vec3 hi = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
    return mix(lo, hi, step(vec3(0.0031308), c));
}

void main() {
    float len = vShape.x;
    float radius = vShape.y;
    float skirt = vShape.z;

    float dx = max(max(-vLocal.x, vLocal.x - len), 0.0);
    float dist = length(vec2(dx, vLocal.y));

    mediump float core = clamp(radius + 0.5 - dist, 0.0, 1.0);
    mediump float glow = 0.0;
    if (skirt > 0.0) {
        mediump float g = 1.0 - clamp((dist - radius) / skirt, 0.0, 1.0);
        glow = 0.35 * g * g;
    }
    mediump float a = core + glow * (1.0 - core);
    mediump vec3 c = vColor * a;
    if (pc.misc.x > 0.5) {
        c = linearToSrgb(clamp(c, 0.0, 1.0));
    }
    outColor = vec4(c, a);
}
