#version 450
// Soft light is evaluated at pixel centres, like every other primitive. It falls to zero inside its quad
// at any intensity (M3 stage C): the Gaussian is faded out over the last fifth of the cut-off radius
// along a circle inside the quad, so no square edge shows however bright the light is.
precision mediump int;
precision highp float;

layout(location = 0) flat in mediump vec3 vColor;
layout(location = 1) in vec2 vPos;
layout(location = 2) flat in vec3 vGlow;

layout(push_constant) uniform Push {
    vec4 view;
    vec4 rot;
    vec4 xform;
    vec4 misc;
} pc;

layout(location = 0) out mediump vec4 outColor;

void main() {
    vec2 d = vPos - vGlow.xy;
    float r2 = dot(d, d) * vGlow.z;
    float lim = pc.misc.x * pc.misc.y;  // the cut-off, in sigmas
    float rs = sqrt(2.0 * r2);          // the distance, in sigmas
    mediump float g = exp(-r2) * (1.0 - smoothstep(0.8 * lim, lim, rs));
    outColor = vec4(vColor * g, g);
}
