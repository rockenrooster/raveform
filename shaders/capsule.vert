#version 450
// Capsule (design/look.md §9): a segment with a half width, a colour per end and a round or flat cut at
// each end. One instance per segment; the quad is expanded from gl_VertexIndex (4-vertex strip).
// Positions are content pixels mapped to logical landscape pixels by xform (scale, offset); the
// pre-rotation (Android) is applied to clip space, so only passes drawn into the swapchain use it.

layout(location = 0) in vec4 inEnds;   // p0.xy, p1.xy (content pixels)
layout(location = 1) in vec4 inC0;     // colour at p0 (linear HDR), half width (content pixels)
layout(location = 2) in vec4 inC1;     // colour at p1, flags (1 flat start, 2 flat end, 4 disc)

layout(push_constant) uniform Push {
    vec4 view;   // logical width, height, 2/width, 2/height
    vec4 rot;    // pre-rotation rows (r00, r01, r10, r11)
    vec4 xform;  // content scale, offset x, offset y, line width factor (look variant)
    vec4 misc;
} pc;

layout(location = 0) flat out vec3 vC0;
layout(location = 1) flat out vec3 vC1;
layout(location = 2) out vec2 vLocal;       // (along, across) the segment in logical pixels
layout(location = 3) flat out vec3 vShape;  // length, half width (logical pixels), flags

void main() {
    vec2 p0 = inEnds.xy * pc.xform.x + pc.xform.yz;
    vec2 p1 = inEnds.zw * pc.xform.x + pc.xform.yz;
    float flags = inC1.w;
    bool disc = mod(floor(flags / 4.0 + 0.001), 2.0) > 0.5;
    float r = inC0.w * pc.xform.x * (disc ? 1.0 : pc.xform.w);

    vec2 d = p1 - p0;
    float len = length(d);
    vec2 dir = len > 1e-4 ? d / len : vec2(1.0, 0.0);
    vec2 nrm = vec2(-dir.y, dir.x);
    float ext = r + 1.5;  // round cap plus the one-pixel edge

    int i = gl_VertexIndex;
    float along = (i & 1) != 0 ? len + ext : -ext;
    float across = (i & 2) != 0 ? ext : -ext;
    vec2 pos = p0 + dir * along + nrm * across;

    vLocal = vec2(along, across);
    vShape = vec3(len, r, flags);
    vC0 = inC0.rgb;
    vC1 = inC1.rgb;

    vec2 ndc = pos * pc.view.zw - 1.0;
    gl_Position = vec4(pc.rot.x * ndc.x + pc.rot.y * ndc.y, pc.rot.z * ndc.x + pc.rot.w * ndc.y, 0.0, 1.0);
}
