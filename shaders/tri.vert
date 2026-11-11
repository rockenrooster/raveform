#version 450
// Flat additive triangle with a colour per corner (glass, design/look.md §4).

layout(location = 0) in vec2 inPos;    // content pixels
layout(location = 1) in vec4 inColor;  // linear HDR colour

layout(push_constant) uniform Push {
    vec4 view;
    vec4 rot;
    vec4 xform;
    vec4 misc;
} pc;

layout(location = 0) out vec3 vColor;

void main() {
    vec2 pos = inPos * pc.xform.x + pc.xform.yz;
    vColor = inColor.rgb;
    vec2 ndc = pos * pc.view.zw - 1.0;
    gl_Position = vec4(pc.rot.x * ndc.x + pc.rot.y * ndc.y, pc.rot.z * ndc.x + pc.rot.w * ndc.y, 0.0, 1.0);
}
