#version 450
// Soft light is evaluated at pixel centres, like every other primitive.
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
    float lim = pc.misc.x * pc.misc.y;  // the square cut-off, in sigmas
    vec2 box = abs(d) * sqrt(2.0 * vGlow.z);
    mediump float g = (box.x > lim || box.y > lim) ? 0.0 : exp(-r2);
    outColor = vec4(vColor * g, g);
}
