#version 450
// Distortion field (design/look.md §7) at W/8 (W/4 on Ultra): displacement of the scene image in
// logical pixels, summed over shock rings (a Gaussian ring profile moving out with the front) and
// gravity wells (a pull toward the centre, strongest at the peak radius, gone by the outer radius).
// The block must match shaders/composite.frag (one uniform buffer feeds both).
precision mediump int;
precision highp float;

layout(std140, set = 0, binding = 3) uniform Composite {
    vec4 view;
    vec4 bgEdge;
    vec4 bgCentre;
    vec4 bgShape;
    vec4 canvas;
    vec4 tone;
    vec4 tone2;
    vec4 vignette;
    vec4 nebCount;
    vec4 neb[16];
    vec4 post;
    vec4 post2;
    vec4 dcount;    // rings, wells
    vec4 ring[16];  // per ring: (x, y, radius, sigma) logical px, (amount px, 0, 0, 0)
    vec4 well[8];   // per well: (x, y, peak radius, gone radius) logical px, (amount px, 0, 0, 0)
} u;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outOffset;

void main() {
    vec2 px = vUv * u.view.xy;
    vec2 d = vec2(0.0);
    int rings = int(u.dcount.x);
    for (int i = 0; i < rings; ++i) {
        vec4 a = u.ring[2 * i];
        vec2 e = px - a.xy;
        float r = length(e);
        if (r < 1e-3) continue;
        float k = (r - a.z) / a.w;
        d += e / r * (u.ring[2 * i + 1].x * exp(-0.5 * k * k));
    }
    int wells = int(u.dcount.y);
    for (int i = 0; i < wells; ++i) {
        vec4 a = u.well[2 * i];
        vec2 e = px - a.xy;
        float r = length(e);
        if (r < 1e-3 || r >= a.w) continue;
        float f = r <= a.z ? r / a.z : (a.w - r) / (a.w - a.z);
        f = f * f * (3.0 - 2.0 * f);
        d -= e / r * (u.well[2 * i + 1].x * f);
    }
    outOffset = vec4(d, 0.0, 0.0);
}
