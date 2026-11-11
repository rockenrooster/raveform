#version 450
// Capsule: the universal line primitive (TECH_SPEC 5.3). One instance = one round-capped segment.
// The quad is expanded from gl_VertexIndex (4-vertex triangle strip); no vertex buffer besides instances.
// Positions are logical landscape pixels (y down). The pre-rotation for Android is a 2x2 matrix applied
// to the final clip-space position, so everything upstream stays in logical landscape space.

layout(location = 0) in vec4 inEnds;      // p0.xy, p1.xy in logical pixels
layout(location = 1) in vec2 inRadInt;    // radius (px), HDR intensity
layout(location = 2) in vec4 inColor;     // sRGB bytes, normalized
layout(location = 3) in uint inFlags;     // bits 0-7: glow skirt width in quarter pixels

layout(push_constant) uniform Push {
    vec4 view;   // logical width, height, 2/width, 2/height
    vec4 rot;    // pre-rotation rows (r00, r01, r10, r11)
    vec4 misc;   // x: encode linear->sRGB in the shader (UNORM swapchain), y: time (s)
} pc;

layout(location = 0) out vec3 vColor;          // linear colour * intensity
layout(location = 1) out vec2 vLocal;          // (along, across) the segment, pixels
layout(location = 2) flat out vec3 vShape;     // length, radius, skirt

vec3 srgbToLinear(vec3 c) {
    vec3 lo = c / 12.92;
    vec3 hi = pow((c + 0.055) / 1.055, vec3(2.4));
    return mix(lo, hi, step(vec3(0.04045), c));
}

void main() {
    vec2 p0 = inEnds.xy;
    vec2 p1 = inEnds.zw;
    vec2 d = p1 - p0;
    float len = length(d);
    vec2 dir = len > 1e-4 ? d / len : vec2(1.0, 0.0);
    vec2 nrm = vec2(-dir.y, dir.x);

    float radius = inRadInt.x;
    float skirt = float(inFlags & 0xFFu) * 0.25;
    float ext = radius + skirt + 1.0;   // +1 px for anti-aliasing

    int i = gl_VertexIndex;
    float along = (i & 1) != 0 ? len + ext : -ext;
    float across = (i & 2) != 0 ? ext : -ext;
    vec2 pos = p0 + dir * along + nrm * across;

    vLocal = vec2(along, across);
    vShape = vec3(len, radius, skirt);
    vColor = srgbToLinear(inColor.rgb) * inRadInt.y;

    vec2 ndc = pos * pc.view.zw - 1.0;
    gl_Position = vec4(pc.rot.x * ndc.x + pc.rot.y * ndc.y,
                       pc.rot.z * ndc.x + pc.rot.w * ndc.y, 0.0, 1.0);
}
