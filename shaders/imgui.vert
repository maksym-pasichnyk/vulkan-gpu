#version 450 core

layout(location = 0) in vec2 in_vert_position;
layout(location = 1) in vec2 in_vert_texcoord;
layout(location = 2) in vec4 in_vert_color;

layout(push_constant) uniform uPushConstant {
    vec2 uScale;
    vec2 uTranslate;
} pc;

layout(location = 0) out vec2 out_vert_texcoord;
layout(location = 1) out vec4 out_vert_color;

void main() {
    out_vert_color = in_vert_color;
    out_vert_texcoord = in_vert_texcoord;

    gl_Position = vec4(in_vert_position * pc.uScale + pc.uTranslate, 0, 1);
}