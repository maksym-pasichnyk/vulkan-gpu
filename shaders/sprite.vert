#version 450

const vec2 _19[4] = vec2[](vec2(0.0), vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0));
const uint _29[6] = uint[](0u, 1u, 2u, 2u, 1u, 3u);

layout(push_constant, std430) uniform SpriteRendererUniforms
{
    mat4 WorldToScreenMatrix;
} _58;

layout(location = 0) in vec2 SpriteLocation;
layout(location = 1) in vec2 SpriteSize;
layout(location = 2) in vec4 SpriteTexCoords;

layout(location = 0) out vec2 TexCoordOutput;

void main()
{
    vec2 VertexPosition = SpriteLocation + (_19[_29[gl_VertexIndex]] * SpriteSize);
    gl_Position = _58.WorldToScreenMatrix * vec4(VertexPosition, 0.0, 1.0);

    float QuadTexCoordX = _19[_29[gl_VertexIndex]].x;
    float QuadTexCoordY = _19[_29[gl_VertexIndex]].y;
    float TexCoordX = mix(SpriteTexCoords.x, SpriteTexCoords.z, QuadTexCoordX);
    float TexCoordY = mix(SpriteTexCoords.y, SpriteTexCoords.w, QuadTexCoordY);
    TexCoordOutput = vec2(TexCoordX, TexCoordY);
}