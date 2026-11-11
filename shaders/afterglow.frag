#version 450
// Afterglow (design/look.md v2.1 §7): a soft tail on hot light only. The source is one bloom level -
// the scene blurred to sigma 6 ref px at one eighth resolution - less a knee on its brightest channel,
// source x max(m - knee, 0) / m. The history (same size) keeps max(source, previous x e^(-dt / tau)),
// the previous history re-aimed by the camera and faded out toward its edges; the second output is
// what the past still holds over the present, max(previous x decay - source, 0), which the composite
// adds (through the gathering pass). A still picture gains nothing; line-sharp history never exists.
precision mediump int;
precision mediump float;

layout(set = 0, binding = 0) uniform mediump sampler2D source;
layout(set = 0, binding = 1) uniform mediump sampler2D history;

layout(push_constant) uniform Push {
    vec4 rot;
    highp vec4 p0;  // camera scale (this frame / last), shift x, shift y (logical px), decay
    highp vec4 p1;  // logical width, height, 1 / width, 1 / height
    vec4 p2;        // knee
} pc;

layout(location = 0) in highp vec2 vUv;
layout(location = 0) out vec4 outHistory;
layout(location = 1) out vec4 outExcess;

void main() {
    vec3 s = texture(source, vUv).rgb;
    float m = max(s.r, max(s.g, s.b));
    s *= max(m - pc.p2.x, 0.0) / max(m, 1e-4);
    // This frame's logical pixel q was at (q - shift) / scale in the previous frame.
    highp vec2 prev = (vUv * pc.p1.xy - pc.p0.yz) / pc.p0.x;
    highp vec2 puv = prev * pc.p1.zw;
    vec3 p = vec3(0.0);
    if (puv.x >= 0.0 && puv.y >= 0.0 && puv.x <= 1.0 && puv.y <= 1.0) {
        // Fade the history out toward the previous frame's edges, so a zoom snap (the Drop) or a jump
        // does not print the old frame's border into the tail.
        highp vec2 edge = min(puv, 1.0 - puv);
        float fade = smoothstep(0.0, 0.04, float(min(edge.x * pc.p1.x * pc.p1.w, edge.y)));
        p = texture(history, puv).rgb * (float(pc.p0.w) * fade);
    }
    outHistory = vec4(max(s, p), 1.0);
    outExcess = vec4(max(p - s, vec3(0.0)), 1.0);
}
