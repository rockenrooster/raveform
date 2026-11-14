#version 450
// Composite (design/look.md §7, §9): distort -> scene + pools + bloom + afterglow + background ->
// vignette -> exposure -> hue-preserving tone map with white bleed -> grade -> (threat layer and HUD are drawn
// after this, in the same pass) -> sRGB encoding with +-half a code value of dither.
// Colour maths in mediump (half precision on phones: twice the ALU rate); positions in highp.
precision mediump int;
precision mediump float;

layout(set = 0, binding = 0) uniform mediump sampler2D scene;
layout(set = 0, binding = 1) uniform mediump sampler2D pools;
layout(set = 0, binding = 2) uniform mediump sampler2D bloom;
layout(set = 0, binding = 4) uniform mediump sampler2D afterglow;   // excess of the past over the present
layout(set = 0, binding = 5) uniform mediump sampler2D distortion;  // displacement, logical px
layout(set = 0, binding = 6) uniform mediump sampler2D background;  // gradient and clouds (shaders/background.frag)

layout(std140, set = 0, binding = 3) uniform Composite {
    highp vec4 view;      // logical width, height, 1/width, 1/height
    vec4 bgEdge;          // gradient edge colour (x level), w: background level (look variant)
    vec4 bgCentre;        // gradient centre colour, w: power
    highp vec4 bgShape;   // centre x, y (fractions of the canvas), radius (canvas heights), canvas aspect W/H
    highp vec4 canvas;    // content scale (logical px per canvas px), offset x, offset y, canvas height
    vec4 tone;            // exposure, white point, bleed amount, bleed start
    vec4 tone2;           // bleed end, bloom strength, 1 = encode sRGB in the shader (UNORM target), dither on
    vec4 vignette;        // amount, inner, outer, on
    highp vec4 nebCount;  // x: clouds, y: frame (dither seed)
    highp vec4 neb[16];   // per cloud: (x, y, sigma, cut-off half size) in canvas px, (r, g, b, 0)
    vec4 post;            // afterglow strength, aberration on, aberration of distortion, distortion on
    vec4 post2;           // aberration at the corners (logical px), aberration snare (logical px), drain toward white
    vec4 dcount;          // distortion rings, wells (shaders/distort.frag)
    highp vec4 ring[16];
    highp vec4 well[8];
    vec4 grade;           // saturation, vibrance, contrast, on (look.md §3 Grade)
} u;

layout(push_constant) uniform Push {
    vec4 rot;
    vec4 p0;
    vec4 p1;
    vec4 p2;
} pc;

layout(location = 0) in highp vec2 vUv;
layout(location = 0) out vec4 outColor;

float encode1(float x) {
    x = clamp(x, 0.0, 1.0);
    return x <= 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}
vec3 encode(vec3 c) { return vec3(encode1(c.r), encode1(c.g), encode1(c.b)); }
// Interleaved gradient noise (TECH_SPEC 5.2) in [-0.5, 0.5) code values, shifted every frame.
float ditherNoise() {
    highp vec2 f = gl_FragCoord.xy + 5.588238 * mod(u.nebCount.y, 64.0);
    highp float ign = fract(52.9829189 * fract(dot(f, vec2(0.06711056, 0.00583715))));
    return (float(ign) - 0.5) / 255.0;
}

void main() {
    // Distortion and chromatic aberration bend the scene image only (look.md §7).
    highp vec2 uvd = vUv;
    vec2 disp = vec2(0.0);
    if (u.post.w > 0.5) {
        disp = texture(distortion, vUv).xy;
        uvd += disp * u.view.zw;
    }
    vec3 c;
    if (u.post.y > 0.5) {
        highp vec2 centre = 0.5 * u.view.xy;
        highp vec2 fromCentre = vUv * u.view.xy - centre;
        highp float dist = length(fromCentre);
        float rr = dist / length(centre);
        vec2 radial = dist > 1e-3 ? vec2(fromCentre / dist) : vec2(0.0);
        highp vec2 split = (u.post.z * disp + radial * (u.post2.x * rr * rr + u.post2.y)) * u.view.zw;
        c = vec3(texture(scene, uvd + split).r, texture(scene, uvd).g, texture(scene, uvd - split).b);
    } else {
        c = texture(scene, uvd).rgb;
    }
    // The bloom texture also carries the pools, the afterglow excess and the background, each divided
    // by the bloom strength (shaders/bloom_final.frag): one tap for every low-resolution term.
    c += u.tone2.y * texture(bloom, vUv).rgb;
    if (u.post2.z > 0.0) c = mix(c, vec3(dot(c, vec3(0.2126, 0.7152, 0.0722))), u.post2.z);

    if (u.vignette.w > 0.5) {
        vec2 d = vUv - 0.5;
        float r = sqrt(2.0 * dot(d, d));  // corners at 1
        c *= 1.0 - u.vignette.x * smoothstep(u.vignette.y, u.vignette.z, r);
    }

    // Tone map: compress the brightest channel (extended Reinhard, white point), keep the hue, bleed
    // very hot values toward white. tone = (exposure, 1 / white point^2, bleed, bleed start),
    // tone2.x = 1 / (bleed end - bleed start).
    c = max(c * u.tone.x, vec3(0.0));
    float m = max(c.r, max(c.g, c.b));
    float tm = min(m * (1.0 + m * u.tone.y) / (1.0 + m), 1.0);
    vec3 base = c * (tm / max(m, 1e-6));
    float b = clamp((m - u.tone.w) * u.tone2.x, 0.0, 1.0);
    float white = b * b * (3.0 - 2.0 * b) * u.tone.z;
    vec3 outLin = mix(base, vec3(tm), white);

    // Grade (look.md §3; the owner's play of 2026-11-14: "a little more color pop, like we did for the Prism powerup"): colour
    // pushed away from its luminance by saturation + vibrance x (1 - its own saturation), so the paler a colour the more it gains
    // and a pure hue keeps its place; then an S-curve on the luminance's square root (0, a half and 1 stay put), hue and
    // saturation kept. A channel that would pass 1 scales the colour down instead (its hue kept). The threat layer and the HUD
    // are drawn after this and keep their own colours.
    if (u.grade.w > 0.5) {
        const vec3 luma = vec3(0.2126, 0.7152, 0.0722);
        float Y = dot(outLin, luma);
        float mx = max(outLin.r, max(outLin.g, outLin.b));
        float mn = min(outLin.r, min(outLin.g, outLin.b));
        float sat = mx > 1e-4 ? (mx - mn) / mx : 0.0;
        vec3 g = max(vec3(Y) + (outLin - vec3(Y)) * (u.grade.x + u.grade.y * (1.0 - sat)), vec3(0.0));
        float Yg = dot(g, luma);
        float p = sqrt(max(Yg, 0.0));
        float q = clamp(p + u.grade.z * p * (1.0 - p) * (2.0 * p - 1.0), 0.0, 1.0);
        g *= Yg > 1e-5 ? (q * q) / Yg : 1.0;
        float gm = max(g.r, max(g.g, g.b));
        outLin = gm > 1.0 ? g / gm : g;
    }

    // +-half a code value of noise in the encoded image before the 8-bit write (look.md §7). On an sRGB
    // target the hardware encodes, so the noise moves to linear light through the curve's slope: exact
    // 2.2749 x^(7/12), here max(1.82 sqrt(x), 1 / 12.92), within -20% / +30% (first order; the
    // second-order term is far below a code value).
    if (u.tone2.z > 0.5) {
        float noise = u.tone2.w > 0.5 ? ditherNoise() : 0.0;
        outColor = vec4(clamp(encode(outLin) + noise, 0.0, 1.0), 1.0);
    } else if (u.tone2.w > 0.5) {
        vec3 slope = max(1.82 * sqrt(outLin), vec3(1.0 / 12.92));
        outColor = vec4(clamp(outLin + ditherNoise() * slope, 0.0, 1.0), 1.0);
    } else {
        outColor = vec4(clamp(outLin, 0.0, 1.0), 1.0);
    }
}
