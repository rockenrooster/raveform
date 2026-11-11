#version 450
// Soft light (design/look.md §3): a Gaussian disc e^(-d^2 / 2 sigma^2), sigma = radius / radiusOverSigma,
// cut off at a square of cutoffRadii x radius. Sparkles are drawn into the scene at full resolution,
// pools into the quarter-resolution pool target; both add to the picture and feed the bloom.

layout(location = 0) in vec3 inGlow;   // centre x, y, radius (content pixels)
layout(location = 1) in vec4 inColor;  // linear HDR colour

layout(push_constant) uniform Push {
    vec4 view;   // logical width, height, 2/width, 2/height
    vec4 rot;
    vec4 xform;  // content scale, offset x, offset y, unused
    vec4 misc;   // cutoff radii, radius over sigma, pool level, pool radius threshold (logical px)
} pc;

layout(location = 0) flat out vec3 vColor;
layout(location = 1) out vec2 vPos;            // logical pixels
layout(location = 2) flat out vec3 vGlow;      // centre (logical), 1 / (2 sigma^2)

void main() {
    vec2 c = inGlow.xy * pc.xform.x + pc.xform.yz;
    float r = inGlow.z * pc.xform.x;
    float sigma = r / pc.misc.y;
    vec2 corner = vec2((gl_VertexIndex & 1) != 0 ? 1.0 : -1.0, (gl_VertexIndex & 2) != 0 ? 1.0 : -1.0);
    vec2 pos = c + corner * (r * pc.misc.x + 1.0);
    vPos = pos;
    vGlow = vec3(c, 1.0 / max(2.0 * sigma * sigma, 1e-6));
    vColor = inColor.rgb * (r >= pc.misc.w ? pc.misc.z : 1.0);
    vec2 ndc = pos * pc.view.zw - 1.0;
    gl_Position = vec4(pc.rot.x * ndc.x + pc.rot.y * ndc.y, pc.rot.z * ndc.x + pc.rot.w * ndc.y, 0.0, 1.0);
}
