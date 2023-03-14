#version 450

layout(set = 0, binding = 0) uniform sampler2D Texture;

layout(location = 0) out vec4 ColorOutput;
layout(location = 0) in vec2 TexCoordInput;

void main()
{
    ColorOutput = texture(Texture, TexCoordInput);
}
