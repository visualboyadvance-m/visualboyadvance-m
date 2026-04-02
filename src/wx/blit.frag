#version 450

layout(set = 0, binding = 0) uniform sampler2D u_tex;

layout(location = 0) in  vec2 i_uv;
layout(location = 0) out vec4 o_color;

void main()
{
    //o_color = texture(u_tex, i_uv);
    o_color = vec4(1, 0, 0, 1);
}
