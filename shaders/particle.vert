#version 450
// Stateless spark (TECH_SPEC 5.3 "Particle", design/look.md §5): written once at birth into a ring
// buffer. Position p = p0 + v0 (1 - e^(-drag t)) / drag, velocity v0 e^(-drag t); drawn as a capsule
// streaked along the velocity (speed x streak seconds, capped), fading with exponent 1.6 over its life.
// Colour field (look.md §2): tail = half its own colour, head = its colour mixed 40% toward white.
// Shares capsule.frag.

layout(location = 0) in vec3 inBirth;   // x, y (world units), birth time (s)
layout(location = 1) in vec4 inMotion;  // velocity x, y (wu/s), life (s), drag (1/s)
layout(location = 2) in vec2 inLight;   // intensity at birth, half width (ref px)
layout(location = 3) in vec4 inColour;  // own colour, linear light

layout(push_constant) uniform Push {
    vec4 view;    // logical width, height, 2/width, 2/height
    vec4 rot;
    vec4 xform;   // logical px per world unit, offset x, offset y, logical px per ref px (x line width)
    vec4 misc;    // now (s), streak seconds, streak cap (wu), fade exponent
    vec4 extra0;  // local player x, y (wu), dim radius (wu), dim factor
    vec4 extra1;
} pc;

layout(location = 0) flat out vec3 vC0;
layout(location = 1) flat out vec3 vC1;
layout(location = 2) out vec2 vLocal;
layout(location = 3) flat out vec3 vShape;

void main() {
    float t = pc.misc.x - inBirth.z;
    float life = inMotion.z;
    if (t < 0.0 || t >= life) {
        // Not born yet or dead: a degenerate quad outside the clip volume.
        vC0 = vec3(0.0);
        vC1 = vec3(0.0);
        vLocal = vec2(0.0);
        vShape = vec3(0.0);
        gl_Position = vec4(-2.0, -2.0, 0.0, 1.0);
        return;
    }
    float drag = inMotion.w;
    float decay = exp(-drag * t);
    float k = drag > 1e-4 ? (1.0 - decay) / drag : t;
    vec2 p = inBirth.xy + inMotion.xy * k;
    vec2 v = inMotion.xy * decay;
    float speed = length(v);
    vec2 dir = speed > 1e-3 ? v / speed : vec2(1.0, 0.0);
    float streak = min(speed * pc.misc.y, pc.misc.z);
    vec2 tail = p - dir * streak;

    float fade = pow(max(1.0 - t / life, 0.0), pc.misc.w) * inLight.x;
    if (length(p - pc.extra0.xy) < pc.extra0.z) fade *= pc.extra0.w;  // readability: dim near the player
    vec3 own = inColour.rgb;
    vC0 = own * 0.5 * fade;
    vC1 = mix(own, vec3(1.0), 0.4) * fade;

    vec2 p0 = tail * pc.xform.x + pc.xform.yz;
    vec2 p1 = p * pc.xform.x + pc.xform.yz;
    float r = inLight.y * pc.xform.w;
    vec2 d = p1 - p0;
    float len = length(d);
    vec2 sdir = len > 1e-4 ? d / len : vec2(1.0, 0.0);
    vec2 nrm = vec2(-sdir.y, sdir.x);
    float ext = r + 1.5;
    int i = gl_VertexIndex & 3;
    float along = (i & 1) != 0 ? len + ext : -ext;
    float across = (i & 2) != 0 ? ext : -ext;
    vec2 pos = p0 + sdir * along + nrm * across;
    vLocal = vec2(along, across);
    vShape = vec3(len, r, 0.0);
    vec2 ndc = pos * pc.view.zw - 1.0;
    gl_Position = vec4(pc.rot.x * ndc.x + pc.rot.y * ndc.y, pc.rot.z * ndc.x + pc.rot.w * ndc.y, 0.0, 1.0);
}
