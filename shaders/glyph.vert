#version 450
// Instanced glyph (TECH_SPEC 5.3 "Glyph", design/look.md §2 and §4). The static mesh holds four corners
// per segment in glyph units; the instance places, turns, scales and colours the whole shape, layer by
// layer: layers 0 and 2 turn with rot.xy, layer 1 (the ornament) with rot.zw. Colour fields are evaluated
// at the two ends of each segment and interpolated along it (look.md §2), then the segment is drawn
// exactly like a capsule (shared fragment shader capsule.frag).

layout(location = 0) in vec4 inSeg;    // segment ends, glyph units, y down
layout(location = 1) in vec4 inInfo;   // extent, layer, joint flags, field (0 faces heading, 1 spins)
layout(location = 2) in vec2 inPos;    // instance: world units
layout(location = 3) in vec4 inRot;    // cos, sin (layers 0 and 2), cos, sin (layer 1)
layout(location = 4) in vec4 inShape;  // world units per glyph unit, hull intensity, hull half width (ref px), mode
layout(location = 5) in vec4 inA;      // colour a, linear light
layout(location = 6) in vec4 inB;      // colour b, linear light

layout(push_constant) uniform Push {
    vec4 view;    // logical width, height, 2/width, 2/height
    vec4 rot;     // pre-rotation rows
    vec4 xform;   // logical px per world unit, offset x, offset y, logical px per ref px (x line width)
    vec4 misc;    // ornament and core half width (ref px), ornament and core intensity (of the hull)
    vec4 extra0;  // palette accent (linear), enemy kick swell (scale multiplier)
    vec4 extra1;  // w: the layer's level (0 = as drawn; see levelled)
} pc;

// A layer's level (pc.extra1.w in (0, 1)): every segment's opacity scaled by it (capsule.vert's levelled: the threat layer
// faded with the run behind the pause, 2026-11-16).
float levelled(float flags) {
    float level = pc.extra1.w;
    if (level <= 0.0 || level >= 0.999) return flags;
    float a8 = floor(flags / 8.0 + 0.001);
    float base = flags - 8.0 * a8;
    float op = (a8 < 0.5 ? 255.0 : a8) * level;
    return base + 8.0 * max(1.0, floor(op + 0.5));
}

layout(location = 0) flat out vec3 vC0;
layout(location = 1) flat out vec3 vC1;
layout(location = 2) out vec2 vLocal;
layout(location = 3) flat out vec3 vShape;

vec3 stops3(float t, vec3 a, vec3 b, vec3 c) {
    t = clamp(t, 0.0, 1.0) * 2.0;
    return t < 1.0 ? mix(a, b, t) : mix(b, c, t - 1.0);
}

// Four colours around the centre, repeating (look.md §2 rings).
vec3 ring4(float t, vec3 s0, vec3 s1, vec3 s2, vec3 s3) {
    t = fract(t) * 4.0;
    float u = fract(t);
    if (t < 1.0) return mix(s0, s1, u);
    if (t < 2.0) return mix(s1, s2, u);
    if (t < 3.0) return mix(s2, s3, u);
    return mix(s3, s0, u);
}

vec3 colourAt(vec2 p, float layer, int mode) {
    const vec3 W = vec3(1.0);
    vec3 a = inA.rgb;
    vec3 b = inB.rgb;
    float ext = max(inInfo.x, 1e-3);
    if (mode == 0) {
        if (layer < 0.5) {
            return inInfo.w < 0.5 ? stops3((ext - p.x) / (2.0 * ext), mix(a, W, 0.4), a, b)  // nose to tail
                                  : stops3(length(p) / ext, b, a, mix(a, W, 0.25));       // centre to rim
        }
        return layer < 1.5 ? pc.extra0.rgb * 0.6 + b * 0.4 : W;
    }
    if (mode == 1) return W;
    if (mode == 2) return layer < 0.5 ? stops3(length(p) / 1.3, W, a, b) : W;
    if (mode == 3) return ring4(atan(p.y, p.x) / 6.2831853, a, W, a, b);
    if (mode == 4) return ring4(atan(p.y, p.x) / 6.2831853, W, a, W, b);
    return a;
}

void main() {
    int mode = int(inShape.w + 0.5);
    float layer = inInfo.y;
    vec2 cs = layer > 0.5 && layer < 1.5 ? inRot.zw : inRot.xy;
    float scale = inShape.x * (mode == 0 ? pc.extra0.w : 1.0);
    vec2 a0 = inSeg.xy, a1 = inSeg.zw;
    vec2 w0 = inPos + vec2(a0.x * cs.x - a0.y * cs.y, a0.x * cs.y + a0.y * cs.x) * scale;
    vec2 w1 = inPos + vec2(a1.x * cs.x - a1.y * cs.y, a1.x * cs.y + a1.y * cs.x) * scale;
    vec2 p0 = w0 * pc.xform.x + pc.xform.yz;
    vec2 p1 = w1 * pc.xform.x + pc.xform.yz;

    bool layered = mode == 0 || mode == 2;
    float intensity = inShape.y;
    float halfWidth = inShape.z;
    if (layered && layer > 0.5) {
        bool ornament = layer < 1.5;
        intensity *= ornament ? pc.misc.z : pc.misc.w;
        halfWidth = ornament ? pc.misc.x : pc.misc.y;
    }
    float r = halfWidth * pc.xform.w;
    float flags = inInfo.z;

    vec2 d = p1 - p0;
    float len = length(d);
    vec2 dir = len > 1e-4 ? d / len : vec2(1.0, 0.0);
    vec2 nrm = vec2(-dir.y, dir.x);
    float ext = r + 1.5;
    int i = gl_VertexIndex & 3;
    float along = (i & 1) != 0 ? len + ext : -ext;
    float across = (i & 2) != 0 ? ext : -ext;
    vec2 pos = p0 + dir * along + nrm * across;

    vLocal = vec2(along, across);
    vShape = vec3(len, r, levelled(flags));
    vC0 = colourAt(a0, layer, mode) * intensity;
    vC1 = colourAt(a1, layer, mode) * intensity;

    vec2 ndc = pos * pc.view.zw - 1.0;
    gl_Position = vec4(pc.rot.x * ndc.x + pc.rot.y * ndc.y, pc.rot.z * ndc.x + pc.rot.w * ndc.y, 0.0, 1.0);
}
