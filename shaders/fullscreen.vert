#version 450
// One triangle over the whole target. uv is in logical landscape space: for the swapchain pass the
// pre-rotation is undone here (logical = R^T physical), offscreen passes use the identity.

layout(push_constant) uniform Push {
    vec4 rot;
    vec4 p0;
    vec4 p1;
    vec4 p2;
} pc;

layout(location = 0) out vec2 vUv;

void main() {
    vec2 phys = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2)) * 2.0 - 1.0;
    vec2 logical = vec2(pc.rot.x * phys.x + pc.rot.z * phys.y, pc.rot.y * phys.x + pc.rot.w * phys.y);
    vUv = logical * 0.5 + 0.5;
    gl_Position = vec4(phys, 0.0, 1.0);
}
