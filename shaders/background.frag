#version 450
// Background (design/look.md §7, §9; TECH_SPEC 5.2 pass 0) at a quarter of the resolution (half on
// Ultra): the radial gradient plus the nebula clouds, evaluated at the pixel centres of the full-size
// frame that this low-resolution pixel covers. Low-frequency light: the composite upsamples it with one
// bilinear tap and adds it after the bloom (it never blooms). The block must match
// shaders/composite.frag (one uniform buffer feeds both).
precision mediump int;
precision highp float;

layout(std140, set = 0, binding = 3) uniform Composite {
    vec4 view;      // logical width, height, 1/width, 1/height
    vec4 bgEdge;    // gradient edge colour, w: background level (applied by the composite)
    vec4 bgCentre;  // gradient centre colour, w: power
    vec4 bgShape;   // centre x, y (fractions of the canvas), radius (canvas heights), canvas aspect W/H
    vec4 canvas;    // content scale (logical px per canvas px), offset x, offset y, canvas height
    vec4 tone;
    vec4 tone2;
    vec4 vignette;
    vec4 nebCount;  // x: clouds
    vec4 neb[16];   // per cloud: (x, y, sigma, cut-off half size) in canvas px, (r, g, b, 0)
    vec4 post;
    vec4 post2;
    vec4 dcount;
    vec4 ring[16];
    vec4 well[8];
} u;

layout(location = 0) in vec2 vUv;
layout(location = 0) out mediump vec4 outColor;

void main() {
    vec2 px = vUv * u.view.xy;
    // Background in canvas pixels (the same mapping as the scene file's content).
    vec2 q = (px - u.canvas.yz) / u.canvas.x;
    vec2 qn = q / vec2(u.canvas.w * u.bgShape.w, u.canvas.w);
    float d = length(vec2((qn.x - u.bgShape.x) * u.bgShape.w, qn.y - u.bgShape.y)) / u.bgShape.z;
    float x = clamp(1.0 - d, 0.0, 1.0);
    float t = u.bgCentre.w == 1.5 ? x * sqrt(x) : pow(x, u.bgCentre.w);
    vec3 bg = mix(u.bgEdge.rgb, u.bgCentre.rgb, t);
    int n = int(u.nebCount.x);
    for (int i = 0; i < n; ++i) {
        vec4 a = u.neb[2 * i];
        vec2 e = q - a.xy;
        if (abs(e.x) < a.w && abs(e.y) < a.w) bg += u.neb[2 * i + 1].rgb * exp(-dot(e, e) / (2.0 * a.z * a.z));
    }
    outColor = vec4(bg, 1.0);
}
