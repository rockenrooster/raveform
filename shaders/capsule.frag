#version 450
// Capsule coverage (design/look.md §9): clamp(half width + 0.5 - distance to the segment, 0, 1); a flat
// end has no cap and multiplies the coverage by clamp(distance past the cut + 0.5, 0, 1), so two flat
// ends that meet add up to exactly one line. Colour runs from one end to the other. The output is
// premultiplied: the scene and the HUD add it, the threat layer blends it over the tone-mapped image
// (ONE, ONE_MINUS_SRC_ALPHA), so a display-linear colour above 1 still brightens the anti-aliased edge
// before the 8-bit target clamps, as in the reference's mix-then-clip.
precision mediump int;
precision highp float;

layout(location = 0) flat in mediump vec3 vC0;
layout(location = 1) flat in mediump vec3 vC1;
layout(location = 2) in vec2 vLocal;
layout(location = 3) flat in vec3 vShape;

layout(location = 0) out mediump vec4 outColor;

void main() {
    float len = vShape.x;
    float r = vShape.y;
    float flags = vShape.z;
    bool flatStart = mod(floor(flags + 0.001), 2.0) > 0.5;
    bool flatEnd = mod(floor(flags / 2.0 + 0.001), 2.0) > 0.5;
    // M3 stage C: an opacity above the flags (flags = ends + disc + 8 x alpha in 1/255; 0 = opaque), for the
    // dark discs under the edge chevrons on the threat layer. Exact in binary16 (at most 2047).
    float a8 = floor(flags / 8.0 + 0.001);
    mediump float opacity = a8 < 0.5 ? 1.0 : a8 / 255.0;
    float along = vLocal.x;
    float beyond = 0.0;
    float cut = 1.0;
    if (flatStart) {
        cut *= clamp(along + 0.5, 0.0, 1.0);
    } else {
        beyond = max(beyond, -along);
    }
    if (flatEnd) {
        cut *= clamp(len - along + 0.5, 0.0, 1.0);
    } else {
        beyond = max(beyond, along - len);
    }
    float dist = length(vec2(beyond, vLocal.y));
    mediump float cover = clamp(r + 0.5 - dist, 0.0, 1.0) * cut;
    mediump float t = len > 1e-4 ? clamp(along / len, 0.0, 1.0) : 0.0;
    mediump vec3 c = mix(vC0, vC1, t);
    outColor = vec4(c * cover, cover) * opacity;
}
