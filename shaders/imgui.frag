#version 450 core

layout(location = 0) in vec2 in_frag_texcoord;
layout(location = 1) in vec4 in_frag_color;

layout(location = 0) out vec4 out_frag_color;

layout(set = 0, binding = 0) uniform sampler2D Texture;

void main() {
    out_frag_color = in_frag_color * texture(Texture, in_frag_texcoord);
}