#version 450

// Push constants: source UV rect + destination NDC rect
layout(push_constant) uniform PC {
    vec4 src_rect;  // (u0, v0, u1, v1)
    vec4 dst_rect;  // (x0, y0, x1, y1) in NDC
} pc;

layout(location = 0) out vec2 o_uv;

void main()
{
    // Verander de bitwise check voor de Y-as (nu op bit 0 ipv bit 1)
    vec2 pos = vec2(
        (gl_VertexIndex & 2) == 0 ? pc.dst_rect.x : pc.dst_rect.z,
        (gl_VertexIndex & 1) == 0 ? pc.dst_rect.y : pc.dst_rect.w
    );
    vec2 uv = vec2(
        (gl_VertexIndex & 2) == 0 ? pc.src_rect.x : pc.src_rect.z,
        (gl_VertexIndex & 1) == 0 ? pc.src_rect.y : pc.src_rect.w
    );

    gl_Position = vec4(pos, 0.0, 1.0);
    o_uv        = uv;
}
