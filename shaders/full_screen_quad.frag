#version 450

layout(location = 0) in vec2 TexCoordInput;
layout(location = 0) out vec4 ColorOutput;

layout(binding = 0) uniform sampler2D Texture;

void main() {
    ColorOutput = texture(Texture, TexCoordInput);
}